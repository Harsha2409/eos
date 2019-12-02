#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/testing/tester.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>

#include <boost/test/unit_test.hpp>

#include <contracts.hpp>

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

struct kv {
   eosio::chain::bytes k;
   eosio::chain::bytes v;
};
FC_REFLECT(kv, (k)(v))

class kv_tester : public tester {
 public:
   kv_tester() {
      produce_blocks(2);

      create_accounts({ N(kvtest), N(kvtest1), N(kvtest2), N(kvtest3), N(kvtest4) });
      produce_blocks(2);

      for(auto account : {N(kvtest), N(kvtest1), N(kvtest2), N(kvtest3), N(kvtest4)}) {
         set_code(account, contracts::kv_test_wasm());
         set_abi(account, contracts::kv_test_abi().data());
      }

      set_code(config::system_account_name, contracts::resources_wasm());
      set_abi(config::system_account_name, contracts::resources_abi().data());

      produce_blocks();

      {
         const auto& accnt = control->db().get<account_object, by_name>(N(kvtest));
         abi_def     abi;
         BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
         abi_ser.set_abi(abi, abi_serializer_max_time);
      }
      {
         const auto& accnt = control->db().get<account_object, by_name>(config::system_account_name);
         abi_def     abi;
         BOOST_TEST_REQUIRE(abi_serializer::to_abi(accnt.abi, abi));
         sys_abi_ser.set_abi(abi, abi_serializer_max_time);
      }

      BOOST_TEST_REQUIRE(set_limit(N(eosio.kvdisk), -1) == "");
      BOOST_TEST_REQUIRE(set_limit(N(eosio.kvdisk), -1, N(kvtest1)) == "");
      BOOST_TEST_REQUIRE(set_limit(N(eosio.kvdisk), -1, N(kvtest2)) == "");
      BOOST_TEST_REQUIRE(set_limit(N(eosio.kvdisk), -1, N(kvtest3)) == "");
      BOOST_TEST_REQUIRE(set_limit(N(eosio.kvdisk), -1, N(kvtest4)) == "");
      produce_block();
   }

   action_result push_action(const action_name& name, const variant_object& data, chain::name account = N(kvtest)) {
      string action_type_name = abi_ser.get_action_type(name);
      action act;
      act.account = account;
      act.name    = name;
      act.data    = abi_ser.variant_to_binary(action_type_name, data, abi_serializer_max_time);
      return base_tester::push_action(std::move(act), N(kvtest).to_uint64_t());
   }

   action_result set_limit(name db, int64_t limit, name account = N(kvtest)) {
      action_name name;
      if(db == N(eosio.kvdisk)) {
         name = N(setdisklimit);
      } else if(db == N(eosio.kvram)) {
         name = N(setramlimit);
      } else {
         BOOST_FAIL("Wring database id");
      }
      string action_type_name = sys_abi_ser.get_action_type(name);
      action act;
      act.account = config::system_account_name;
      act.name    = name;
      act.data    = sys_abi_ser.variant_to_binary(action_type_name, mvo()("account", account)("limit", limit), abi_serializer_max_time);
      return base_tester::push_action(std::move(act), config::system_account_name.to_uint64_t());
   }

   void erase(const char* error, name db, name contract, const char* k) {
      BOOST_REQUIRE_EQUAL(error, push_action(N(erase), mvo()("db", db)("contract", contract)("k", k)));
   }

   template <typename V>
   void get(const char* error, name db, name contract, const char* k, V v) {
      BOOST_REQUIRE_EQUAL(error, push_action(N(get), mvo()("db", db)("contract", contract)("k", k)("v", v)));
   }

   template <typename V>
   action_result set(name db, name contract, const char* k, V v) {
      return push_action(N(set), mvo()("db", db)("contract", contract)("k", k)("v", v));
   }

   void setmany(const char* error, name db, name contract, const std::vector<kv>& kvs) {
      BOOST_REQUIRE_EQUAL(error, push_action(N(setmany), mvo()("db", db)("contract", contract)("kvs", kvs), contract));
   }

   template <typename T>
   void scan(const char* error, name db, name contract, const char* prefix, T lower, const std::vector<kv>& expected) {
      BOOST_REQUIRE_EQUAL(error, push_action(N(scan), mvo()("db", db)("contract", contract)("prefix", prefix)(
                                                            "lower", lower)("expected", expected)));
   }

   template <typename T>
   action_result scanrev(name db, name contract, const char* prefix, T lower,
                const std::vector<kv>& expected) {
      return push_action(N(scanrev), mvo()("db", db)("contract", contract)("prefix", prefix)(
                                           "lower", lower)("expected", expected));
   }

   void itstaterased(const char* error, name db, name contract, const char* prefix, const char* k, const char* v,
                     int test_id, bool insert, bool reinsert) {
      BOOST_REQUIRE_EQUAL(error, push_action(N(itstaterased), mvo()("db", db)("contract", contract)("prefix", prefix)(
                                                                    "k", k)("v", v)("test_id", test_id)("insert", insert)("reinsert", reinsert)));
   }

   uint64_t get_usage(name db) {
      if (db == N(eosio.kvram)) {
         return control->get_resource_limits_manager().get_account_ram_usage(N(kvtest));
      } else if (db == N(eosio.kvdisk)) {
         return control->get_resource_limits_manager().get_account_disk_usage(N(kvtest));
      }
      BOOST_FAIL("Wrong db");
      return 0;
   }

   void test_basic(name db) {
      get("", db, N(kvtest), "", nullptr);
      BOOST_TEST("" == set(db, N(kvtest), "", ""));
      get("", db, N(kvtest), "", "");
      BOOST_TEST("" == set(db, N(kvtest), "", "1234"));
      get("", db, N(kvtest), "", "1234");
      get("", db, N(kvtest), "00", nullptr);
      BOOST_TEST("" == set(db, N(kvtest), "00", "aabbccdd"));
      get("", db, N(kvtest), "00", "aabbccdd");
      get("", db, N(kvtest), "02", nullptr);
      erase("", db, N(kvtest), "02");
      get("", db, N(kvtest), "02", nullptr);
      BOOST_TEST("" == set(db, N(kvtest), "02", "42"));
      get("", db, N(kvtest), "02", "42");
      get("", db, N(kvtest), "01020304", nullptr);
      BOOST_TEST("" == set(db, N(kvtest), "01020304", "aabbccddee"));
      erase("", db, N(kvtest), "02");

      get("", db, N(kvtest), "01020304", "aabbccddee");
      get("", db, N(kvtest), "", "1234");
      get("", db, N(kvtest), "00", "aabbccdd");
      get("", db, N(kvtest), "02", nullptr);
      get("", db, N(kvtest), "01020304", "aabbccddee");
   }

  void test_scan(name db, name account = N(kvtest)) {
      setmany("", db, account,
              {
                    kv{ {}, { 0x12 } },
                    kv{ { 0x22 }, {} },
                    kv{ { 0x22, 0x11 }, { 0x34 } },
                    kv{ { 0x22, 0x33 }, { 0x18 } },
                    kv{ { 0x44 }, { 0x76 } },
                    kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                    kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                    kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
              });

      // no prefix, no lower bound
      scan("", db, account, "", nullptr,
           {
                 kv{ {}, { 0x12 } },
                 kv{ { 0x22 }, {} },
                 kv{ { 0x22, 0x11 }, { 0x34 } },
                 kv{ { 0x22, 0x33 }, { 0x18 } },
                 kv{ { 0x44 }, { 0x76 } },
                 kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                 kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                 kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
           });

      // no prefix, lower bound = ""
      scan("", db, account, "", "",
           {
                 kv{ {}, { 0x12 } },
                 kv{ { 0x22 }, {} },
                 kv{ { 0x22, 0x11 }, { 0x34 } },
                 kv{ { 0x22, 0x33 }, { 0x18 } },
                 kv{ { 0x44 }, { 0x76 } },
                 kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                 kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                 kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
           });

      // no prefix, lower bound = "221100"
      scan("", db, account, "", "221100",
           {
                 kv{ { 0x22, 0x33 }, { 0x18 } },
                 kv{ { 0x44 }, { 0x76 } },
                 kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                 kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                 kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
           });

      // no prefix, lower bound = "2233"
      scan("", db, account, "", "2233",
           {
                 kv{ { 0x22, 0x33 }, { 0x18 } },
                 kv{ { 0x44 }, { 0x76 } },
                 kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                 kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                 kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
           });

      // prefix = "22", no lower bound
      scan("", db, account, "22", nullptr,
           {
                 kv{ { 0x22 }, {} },
                 kv{ { 0x22, 0x11 }, { 0x34 } },
                 kv{ { 0x22, 0x33 }, { 0x18 } },
           });

      // prefix = "22", lower bound = "2211"
      scan("", db, account, "22", "2211",
           {
                 kv{ { 0x22, 0x11 }, { 0x34 } },
                 kv{ { 0x22, 0x33 }, { 0x18 } },
           });

      // prefix = "22", lower bound = "2234"
      scan("", db, account, "22", "2234", {});

      // prefix = "33", no lower bound
      scan("", db, account, "33", nullptr, {});

      // prefix = "44", lower bound = "223300"
      // kv_it_lower_bound finds "44", which is in the prefix range
      scan("", db, account, "44", "223300",
           {
                 kv{ { 0x44 }, { 0x76 } },
                 kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                 kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                 kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
           });

      // prefix = "44", lower bound = "2233"
      // kv_it_lower_bound finds "2233", which is out of the prefix range
      scan("", db, account, "44", "2233", {});
   } // test_scan

   void test_scanrev(name db, name account) {
      setmany("", db, account,
              {
                    kv{ {}, { 0x12 } },
                    kv{ { 0x22 }, {} },
                    kv{ { 0x22, 0x11 }, { 0x34 } },
                    kv{ { 0x22, 0x33 }, { 0x18 } },
                    kv{ { 0x44 }, { 0x76 } },
                    kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                    kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                    kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
              });

      // no prefix, no lower bound
      BOOST_TEST("" == scanrev(db, account, "", nullptr,
              {
                    kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
                    kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                    kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                    kv{ { 0x44 }, { 0x76 } },
                    kv{ { 0x22, 0x33 }, { 0x18 } },
                    kv{ { 0x22, 0x11 }, { 0x34 } },
                    kv{ { 0x22 }, {} },
                    kv{ {}, { 0x12 } },
              }));

      // no prefix, lower bound = "4402"
      BOOST_TEST("" == scanrev(db, account, "", "4402",
              {
                    kv{ { 0x44, 0x02 }, { 0x78, 0x04 } },
                    kv{ { 0x44, 0x01 }, { 0x33, 0x22, 0x11 } },
                    kv{ { 0x44, 0x00 }, { 0x11, 0x22, 0x33 } },
                    kv{ { 0x44 }, { 0x76 } },
                    kv{ { 0x22, 0x33 }, { 0x18 } },
                    kv{ { 0x22, 0x11 }, { 0x34 } },
                    kv{ { 0x22 }, {} },
                    kv{ {}, { 0x12 } },
              }));

      // no prefix, lower bound = "44"
      BOOST_TEST("" == scanrev(db, account, "", "44",
              {
                    kv{ { 0x44 }, { 0x76 } },
                    kv{ { 0x22, 0x33 }, { 0x18 } },
                    kv{ { 0x22, 0x11 }, { 0x34 } },
                    kv{ { 0x22 }, {} },
                    kv{ {}, { 0x12 } },
              }));

      // prefix = "22", no lower bound
      BOOST_TEST("" == scanrev(db, account, "22", nullptr,
              {
                    kv{ { 0x22, 0x33 }, { 0x18 } },
                    kv{ { 0x22, 0x11 }, { 0x34 } },
                    kv{ { 0x22 }, {} },
              }));

      // prefix = "22", lower bound = "2233"
      BOOST_TEST("" == scanrev(db, account, "22", "2233",
              {
                    kv{ { 0x22, 0x33 }, { 0x18 } },
                    kv{ { 0x22, 0x11 }, { 0x34 } },
                    kv{ { 0x22 }, {} },
              }));

      // prefix = "22", lower bound = "2234"
      BOOST_TEST("" == scanrev(db, account, "22", "2234", {}));

      // prefix = "33", no lower bound
      BOOST_TEST("" == scanrev(db, account, "33", nullptr, {}));
   } // test_scanrev

   void test_scanrev2(name db) {
      setmany("", db, N(kvtest),
              {
                    kv{ { 0x00, char(0xFF), char(0xFF) }, { 0x10 } },
                    kv{ { 0x01 }, { 0x20 } },
                    kv{ { 0x01, 0x00 }, { 0x30 } },
              });

      // prefix = "00FFFF", no lower bound
      BOOST_TEST("" == scanrev(db, N(kvtest), "00FFFF", nullptr,
              {
                    kv{ { 0x00, char(0xFF), char(0xFF) }, { 0x10 } },
              }));
   } // test_scanrev2

   void test_iterase(name db) {
      for(bool reinsert : {false, true}) {
         // pre-inserted
         for(bool insert : {false, true}) {
            for(int i = 0; i < 8; ++i) {
               setmany("", db, N(kvtest), { kv{ { 0x22 }, { 0x12 } } });
               produce_block();
               itstaterased("Iterator to erased element", db, N(kvtest), "", "22", "12", i, insert, reinsert );
            }
            setmany("", db, N(kvtest), { kv{ { 0x22 }, { 0x12 } } });
            produce_block();
            itstaterased("", db, N(kvtest), "", "22", "12", 8, insert, reinsert );
            if(!reinsert) {
               setmany("", db, N(kvtest), { kv{ { 0x22 }, { 0x12 } } });
               produce_block();
               itstaterased("", db, N(kvtest), "", "22", "12", 9, insert, reinsert );
            }
            setmany("", db, N(kvtest), { kv{ { 0x22 }, { 0x12 } }, kv{ { 0x23 }, { 0x13 } } });
            produce_block();
            itstaterased("", db, N(kvtest), "", "22", "12", 10, insert, reinsert );
            erase("", db, N(kvtest), "23");
            setmany("", db, N(kvtest), { kv{ { 0x22 }, { 0x12 } } });
            produce_block();
            itstaterased("", db, N(kvtest), "", "22", "12", 11, insert, reinsert );
         }
         // inserted inside contract
         for(int i = 0; i < 8; ++i) {
            erase("", db, N(kvtest), "22");
            produce_block();
            itstaterased("Iterator to erased element", db, N(kvtest), "", "22", "12", i, true, reinsert );
         }
         erase("", db, N(kvtest), "22");
         produce_block();
         itstaterased("", db, N(kvtest), "", "22", "12", 8, true, reinsert );
         if(!reinsert) {
            erase("", db, N(kvtest), "22");
            produce_block();
            itstaterased("", db, N(kvtest), "", "22", "12", 9, true, reinsert );
         }
         erase("", db, N(kvtest), "22");
         setmany("", db, N(kvtest), { kv{ { 0x23 }, { 0x13 } } });
         produce_block();
         itstaterased("", db, N(kvtest), "", "22", "12", 10, true, reinsert );
         erase("", db, N(kvtest), "23");
         erase("", db, N(kvtest), "22");
         produce_block();
         itstaterased("", db, N(kvtest), "", "22", "12", 11, true, reinsert );
      }
   }

   void test_ram_usage(name db) {
      uint64_t base_usage = get_usage(db);
      // DISK should start at 0, because it's only used by the kv store.
      if(db == N(eosio.kvdisk)) BOOST_TEST(base_usage == 0);

      get("", db, N(kvtest), "11", nullptr);
      BOOST_TEST(get_usage(db) == base_usage);
      BOOST_TEST("" == set(db, N(kvtest), "11", ""));
      BOOST_TEST(get_usage(db) == base_usage + 112 + 1);
      BOOST_TEST("" == set(db, N(kvtest), "11", "1234"));
      BOOST_TEST(get_usage(db) == base_usage + 112 + 1 + 2);
      BOOST_TEST("" == set(db, N(kvtest), "11", "12"));
      BOOST_TEST(get_usage(db) == base_usage + 112 + 1 + 1);
      erase("", db, N(kvtest), "11");
      BOOST_TEST(get_usage(db) == base_usage);
   }

   void test_resource_limit(name db) {
      uint64_t base_usage = get_usage(db);
      // insert a new element
      BOOST_TEST_REQUIRE(set_limit(db, base_usage + 112) == "");
      BOOST_TEST(set(db, N(kvtest), "11", "").find("account kvtest has insufficient") == 0);
      BOOST_TEST_REQUIRE(set_limit(db, base_usage + 112 + 1) == "");
      BOOST_TEST("" == set(db, N(kvtest), "11", ""));
      // increase the size of a value
      BOOST_TEST_REQUIRE(set_limit(db, base_usage + 112 + 1 + 2 - 1) == "");
      BOOST_TEST(set(db, N(kvtest), "11", "1234").find("account kvtest has insufficient") == 0);
      BOOST_TEST_REQUIRE(set_limit(db, base_usage + 112 + 1 + 2) == "");
      BOOST_TEST("" == set(db, N(kvtest), "11", "1234"));
      // decrease the size of a value
      BOOST_TEST("" == set(db, N(kvtest), "11", ""));
      // decrease limits
      BOOST_TEST(set_limit(db, base_usage + 112).find("account kvtest has insufficient") == 0);
      BOOST_TEST(set_limit(db, base_usage + 112 + 1) == "");
      // erase an element
      erase("", db, N(kvtest), "11");
   }

   abi_serializer abi_ser;
   abi_serializer sys_abi_ser;
};

BOOST_AUTO_TEST_SUITE(kv_tests)

BOOST_FIXTURE_TEST_CASE(kv_basic, kv_tester) try {
   BOOST_REQUIRE_EQUAL("Bad key-value database ID", push_action(N(itlifetime), mvo()("db", N(oops))));
   BOOST_REQUIRE_EQUAL("", push_action(N(itlifetime), mvo()("db", N(eosio.kvram))));
   BOOST_REQUIRE_EQUAL("", push_action(N(itlifetime), mvo()("db", N(eosio.kvdisk))));
   test_basic(N(eosio.kvram));
   test_basic(N(eosio.kvdisk));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(kv_scan, kv_tester) try { //
   // four possibilities depending on whether the next or previous contract table has elements
   test_scan(N(eosio.kvram), N(kvtest2));
   test_scan(N(eosio.kvram), N(kvtest4));
   test_scan(N(eosio.kvram), N(kvtest3));
   test_scan(N(eosio.kvram), N(kvtest1));
   test_scan(N(eosio.kvdisk), N(kvtest2));
   test_scan(N(eosio.kvdisk), N(kvtest4));
   test_scan(N(eosio.kvdisk), N(kvtest3));
   test_scan(N(eosio.kvdisk), N(kvtest1));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(kv_scan_ram_after_disk, kv_tester) try { //
   // Make sure that the behavior of one database is not affected by having the other database populated.
   test_scan(N(eosio.kvdisk), N(kvtest2));
   test_scan(N(eosio.kvdisk), N(kvtest4));
   test_scan(N(eosio.kvdisk), N(kvtest3));
   test_scan(N(eosio.kvdisk), N(kvtest1));
   test_scan(N(eosio.kvram), N(kvtest2));
   test_scan(N(eosio.kvram), N(kvtest4));
   test_scan(N(eosio.kvram), N(kvtest3));
   test_scan(N(eosio.kvram), N(kvtest1));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(kv_scanrev, kv_tester) try { //
   test_scanrev(N(eosio.kvram), N(kvtest2));
   test_scanrev(N(eosio.kvram), N(kvtest4));
   test_scanrev(N(eosio.kvram), N(kvtest3));
   test_scanrev(N(eosio.kvram), N(kvtest1));
   test_scanrev(N(eosio.kvdisk), N(kvtest2));
   test_scanrev(N(eosio.kvdisk), N(kvtest4));
   test_scanrev(N(eosio.kvdisk), N(kvtest3));
   test_scanrev(N(eosio.kvdisk), N(kvtest1));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(kv_scanrev_ram_after_disk, kv_tester) try { //
   test_scanrev(N(eosio.kvdisk), N(kvtest2));
   test_scanrev(N(eosio.kvdisk), N(kvtest4));
   test_scanrev(N(eosio.kvdisk), N(kvtest3));
   test_scanrev(N(eosio.kvdisk), N(kvtest1));
   test_scanrev(N(eosio.kvram), N(kvtest2));
   test_scanrev(N(eosio.kvram), N(kvtest4));
   test_scanrev(N(eosio.kvram), N(kvtest3));
   test_scanrev(N(eosio.kvram), N(kvtest1));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(kv_scanrev2, kv_tester) try { //
   test_scanrev2(N(eosio.kvram));
   test_scanrev2(N(eosio.kvdisk));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(kv_iterase, kv_tester) try { //
   test_iterase(N(eosio.kvram));
   test_iterase(N(eosio.kvdisk));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(kv_ram_usage, kv_tester) try { //
   test_ram_usage(N(eosio.kvram));
   test_ram_usage(N(eosio.kvdisk));
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(kv_resource_limit, kv_tester) try { //
   test_resource_limit(N(eosio.kvram));
   test_resource_limit(N(eosio.kvdisk));
}
FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
