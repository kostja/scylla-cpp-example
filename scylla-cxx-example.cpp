// cqlsh> CREATE KEYSPACE IF NOT EXISTS test WITH replication = {'class': 'SimpleStrategy', 'replication_factor': '3'};
// cqlsh> CREATE TABLE test.test ( lock_key int, bucket_id int, PRIMARY KEY (lock_key, bucket_id) ) WITH CLUSTERING ORDER BY (bucket_id ASC) AND bloom_filter_fp_chance = 0.01 AND caching = {'keys': 'ALL', 'rows_per_partition': 'ALL'}  ;

// g++ lwt_collider.cpp ./cpp-driver/build/libscylla-cpp-driver.so -pthread -Wl,-rpath,./cpp-driver/build/ -I ./cpp-driver/include/ -o lwt_collider

#include <cassandra.h>
#include <algorithm>
#include <stdio.h>
#include <set>
#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <string>

const int ITERATIONS = 1000;
const int THREADS = 32;

std::set<int> track_set;
std::mutex track_set_lock;
std::mutex start_barrier;

void clear_set() {
	std::lock_guard<std::mutex> guard(track_set_lock);
	track_set.clear();
}

void track(int key) {
	std::lock_guard<std::mutex> guard(track_set_lock);

	if (!track_set.insert(key).second) {
		std::cerr << "================= DUPLICATE FOUND: " << key << " =================" << std::endl;
	}
}

void statement(CassSession* session, std::string str) {
	/* Build statement and execute query */
	CassStatement* statement = cass_statement_new(str.c_str(), 0);

	CassFuture* result_future = cass_session_execute(session, statement);

	if (cass_future_error_code(result_future) == CASS_OK) {
		/* Retrieve result set and get the first row */
		const CassResult* result = cass_future_get_result(result_future);

		cass_result_free(result);
	} else {
		/* Handle error */
		const char* message;
		size_t message_length;
		cass_future_error_message(result_future, &message, &message_length);
		fprintf(stderr, "Unable to run statement: '%.*s'\n", (int)message_length, message);
	}

	cass_statement_free(statement);
	cass_future_free(result_future);
}

void insert(CassSession* session, int n) {
	std::string query = "INSERT INTO test.test (lock_key, bucket_id) VALUES(3, "
		+ std::to_string(n) + ") IF NOT EXISTS USING TTL 60";
	/* Build statement and execute query */
	CassStatement* statement = cass_statement_new(query.c_str(), 0);

	CassFuture* result_future = cass_session_execute(session, statement);

	if (cass_future_error_code(result_future) == CASS_OK) {
		/* Retrieve result set and get the first row */
		const CassResult* result = cass_future_get_result(result_future);
		const CassRow* row = cass_result_first_row(result);

		if (row) {
			const CassValue* value = cass_row_get_column_by_name(row, "[applied]");

			cass_bool_t applied = cass_false;
			cass_value_get_bool(value, &applied);
			if (applied == cass_true) {
				track(n);
			}
		}

		cass_result_free(result);
	} else {
		/* Handle error */
		const char* message;
		size_t message_length;
		cass_future_error_message(result_future, &message, &message_length);
		fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
	}

	cass_statement_free(statement);
	cass_future_free(result_future);
}

static std::once_flag truncate_once;

void test_thread(const char *hosts) {
	std::vector<int> keys;
	keys.resize(ITERATIONS);
	for (int i = 0; i < ITERATIONS; ++i) {
		keys[i] = i;
	}
	std::random_shuffle(keys.begin(), keys.end());
	/* Setup and connect to cluster */
	CassFuture* connect_future = NULL;
	CassCluster* cluster = cass_cluster_new();
	CassSession* session = cass_session_new();

	const char* username = "cassandra";
	const char* password = "cassandra";

	cass_cluster_set_credentials(cluster, username, password);

	cass_cluster_set_serial_consistency(cluster, CASS_CONSISTENCY_LOCAL_SERIAL);

	/* Add contact points */
	cass_cluster_set_contact_points(cluster, hosts);

	/* Provide the cluster object as configuration to connect the session */
	connect_future = cass_session_connect(session, cluster);

	if (cass_future_error_code(connect_future) == CASS_OK) {

		{
			std::lock_guard<std::mutex> guard(start_barrier);
			std::call_once(truncate_once, [] (CassSession *session) -> void {
				       return statement(session, "truncate table test.test");
		        }, session);
		}
		for (int i = 0; i < ITERATIONS; i++) {
			insert(session, keys[i]);
		}
	} else {
		/* Handle error */
		const char* message;
		size_t message_length;
		cass_future_error_message(connect_future, &message, &message_length);
		fprintf(stderr, "Unable to connect: '%.*s'\n", (int)message_length, message);
	}

	/* Close the session */
	CassFuture* close_future = cass_session_close(session);
	cass_future_wait(close_future);
	cass_future_free(close_future);

	cass_future_free(connect_future);
	cass_session_free(session);
	cass_cluster_free(cluster);
}

int main(int argc, char* argv[]) {
	// cass_log_set_level(CASS_LOG_DEBUG);

	const char* hosts = "127.0.0.1";
	if (argc > 1) {
		hosts = argv[1];
	}

	std::vector<std::shared_ptr<std::thread>> threads;
	for (int i = 0; i < THREADS; i++) {
		threads.push_back(std::make_shared<std::thread>(test_thread, hosts));
	}
	for (int i = 0; i < THREADS; i++) {
		threads[i]->join();
	}

	return 0;
}
