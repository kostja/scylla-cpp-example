#include <iostream>
#include <string_view>
#include <assert.h>
#include <algorithm>
#include <chrono>
#include <time.h>
#include <iomanip>

class UUID {
private:
    int64_t most_sig_bits;
    int64_t least_sig_bits;
public:

    // UUID timestamp time component is represented in intervals
    // of 1/10 of a microsecond since the beginning of GMT epoch.
    using decimicroseconds = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;
    using milliseconds = std::chrono::milliseconds;


    // A grand day! millis at 00:00:00.000 15 Oct 1582.
    static constexpr decimicroseconds START_EPOCH = decimicroseconds{-122192928000000000L};   

    constexpr UUID() : most_sig_bits(0), least_sig_bits(0) {}
    constexpr UUID(int64_t most_sig_bits, int64_t least_sig_bits)
        : most_sig_bits(most_sig_bits), least_sig_bits(least_sig_bits) {}
    explicit UUID(const std::string& uuid_string) : UUID(std::string_view(uuid_string)) { }
    explicit UUID(const char * s) : UUID(std::string_view(s)) {}
    explicit UUID(std::string_view uuid_string);

    int64_t get_most_significant_bits() const {
        return most_sig_bits;
    }
    int64_t get_least_significant_bits() const {
        return least_sig_bits;
    }
    int version() const {
        return (most_sig_bits >> 12) & 0xf;
    }

    bool is_timestamp() const {
        return version() == 1;
    }

    int64_t timestamp() const {
        assert(is_timestamp());

        return ((most_sig_bits & 0xFFF) << 48) |
               (((most_sig_bits >> 16) & 0xFFFF) << 32) |
               (((uint64_t)most_sig_bits) >> 32);
    }

    milliseconds unix_timestamp()
    {
	    return std::chrono::duration_cast<milliseconds>(decimicroseconds(timestamp()) + START_EPOCH);
    }

    bool operator==(const UUID& v) const {
        return most_sig_bits == v.most_sig_bits
                && least_sig_bits == v.least_sig_bits
                ;
    }
    bool operator!=(const UUID& v) const {
        return !(*this == v);
    }
};


UUID::UUID(std::string_view uuid) {
	std::string us(uuid.begin(), uuid.end());
	us.erase(std::remove(us.begin(), us.end(), '-'), us.end());
	auto size = us.size() / 2;
	if (size != 16) {
		throw std::runtime_error("UUID string size mismatch");
	}
	std::string most = std::string(us.begin(), us.begin() + size);
	std::string least = std::string(us.begin() + size, us.end());
	int base = 16;
	this->most_sig_bits = std::stoull(most, nullptr, base);
	this->least_sig_bits = std::stoull(least, nullptr, base);
}

void print_ballot_time(const char *str) {
	auto time = UUID(str).unix_timestamp();
	time_t seconds = time.count()/1000;
	struct tm tm;
	gmtime_r(&seconds, &tm);
	std::cout << tm.tm_hour << ":" << tm.tm_min << ":" << tm.tm_sec << "." << std::setfill('0') << std::setw(4) << time.count() % 1000 << std::endl;
}

int main(int argc, char* argv[]) {

	print_ballot_time("0c8b896a-e1af-11eb-301f-b4efc9de2c96");
	print_ballot_time("0c8c7b04-e1af-11eb-d0df-7db40addb908");
	print_ballot_time("0c871c36-e1af-11eb-3a30-724efe130ff7");
	return 0;
}
