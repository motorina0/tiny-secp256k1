#include <iostream>
#include <vector>
#include "utils.hpp"

struct IP { uint8_t_32 a = {}; bool e = false; };
struct PA { uint8_t_32 a; uint8_t_32 b; uint8_t_32 e = Null<uint8_t_32>(); std::string except = ""; std::string desc = ""; };

void generate (std::ostream& o) {
	///////////////////////////////// isPrivate
	std::vector<IP> ip;

	// edge cases (verify)
	//   from https://github.com/bitcoin-core/secp256k1/blob/6ad5cdb42a1a8257289a0423d644dcbdeab0f83c/src/tests.c
	ip.push_back({ ZERO, false }); // #L3145, fail, == 0
	ip.push_back({ ONE, true }); // #L3153, OK, > 0
	ip.push_back({ GROUP_ORDER_LESS_1, true }); // #L3171, OK == G - 1
	ip.push_back({ GROUP_ORDER, false }); // #L3115, fail, == G
	ip.push_back({ GROUP_ORDER_OVER_1, false }); // #L3162, fail, >= G
	ip.push_back({ UINT256_MAX, false }); // #L3131, fail, > G

	// fuzz
	for (size_t i = 0; i < 1000; ++i) {
		ip.push_back({ randomPrivate(), true });
	}
	// fuzz (high key)
	for (size_t i = 0; i < 10000; ++i) {
		const auto key = randomScalarHigh();
		const auto verified = secp256k1_ec_seckey_verify(ctx, key.data());

		ip.push_back({ key, verified });
	}

	///////////////////////////////// privateAdd
	std::vector<PA> pa;

	// visually inspected
	//   covers https://github.com/bitcoin-core/secp256k1/blob/6ad5cdb42a1a8257289a0423d644dcbdeab0f83c/src/tests.c
	pa.push_back({ ONE, ZERO, ONE, "", "1 + 0 == 1" });
	for (size_t i = 1; i < 5; ++i) pa.push_back({ ONE, scalarFromUInt32(i), scalarFromUInt32(1 + i) });
	for (size_t i = 1; i < 5; ++i) pa.push_back({ scalarFromUInt32(i), TWO, scalarFromUInt32(i + 2) });

	pa.push_back({ ONE, GROUP_ORDER_LESS_1, Null<uint8_t_32>(), "", "1 + -1 == 0" });
	pa.push_back({ ONE, GROUP_ORDER_LESS_2, GROUP_ORDER_LESS_1 });
	pa.push_back({ ONE, GROUP_ORDER_LESS_3, GROUP_ORDER_LESS_2 });
	pa.push_back({ GROUP_ORDER_LESS_1, GROUP_ORDER_LESS_1, GROUP_ORDER_LESS_2 });
	pa.push_back({ GROUP_ORDER_LESS_2, GROUP_ORDER_LESS_1, GROUP_ORDER_LESS_3 });
	pa.push_back({ GROUP_ORDER_LESS_3, ONE, GROUP_ORDER_LESS_2 });
	pa.push_back({ GROUP_ORDER_LESS_3, TWO, GROUP_ORDER_LESS_1 });
	pa.push_back({ GROUP_ORDER_LESS_3, THREE, Null<uint8_t_32>(), "", "-3 + 3 == 0" });

	// fuzz
	for (size_t i = 0; i < 10000; ++i) {
		const auto paPush = [&](const auto k, const auto t) {
			bool ok = true;
			const auto expected = _privAdd(k, t, ok);
			if (ok) pa.push_back({ k, t, expected });
			else pa.push_back({ k, t, Null<uint8_t_32>() });
		};

		paPush(randomPrivate(), randomPrivate());
		paPush(randomPrivateHigh(), randomPrivateLow());
		paPush(randomPrivateLow(), randomPrivateHigh());
	}

	std::vector<PA> paf;
	paf.push_back({ ZERO, ONE, {}, "Expected Private", "Private key == 0" });
	paf.push_back({ GROUP_ORDER, ONE, {}, "Expected Private", "Private key >= G" });
	paf.push_back({ GROUP_ORDER_OVER_1, ONE, {}, "Expected Private", "Private key >= G" });
	paf.push_back({ ONE, GROUP_ORDER, {}, "Expected Tweak", "Tweak >= G" });
	paf.push_back({ ONE, GROUP_ORDER_OVER_1, {}, "Expected Tweak", "Tweak >= G" });

	// dump JSON
	const auto jPA = [](auto x) {
		return jsonifyO({
			x.desc.empty() ? "" : jsonp("description", jsonify(x.desc)),
			jsonp("priv", jsonify(x.a)),
			jsonp("tweak", jsonify(x.b)),
			x.except.empty() ? jsonp("expected", isNull(x.e) ? "null" : jsonify(x.e)) : "",
			x.except.empty() ? "" : jsonp("exception", jsonify(x.except))
		});
	};

	o << jsonifyO({
		jsonp("valid", jsonifyO({
			jsonp("isPrivate", jsonifyA(ip, [](auto x) {
				return jsonifyO({
					jsonp("priv", jsonify(x.a)),
					jsonp("expected", jsonify(x.e))
				});
			})),
			jsonp("privateAdd", jsonifyA(pa, jPA))
		})),
		jsonp("invalid", jsonifyO({
			jsonp("privateAdd", jsonifyA(paf, jPA))
		}))
	});
}

int main () {
	_ec_init();
	generate(std::cout);

	return 0;
}