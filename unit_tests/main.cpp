#include <gtest/gtest.h>

/*TODO: Create mock library to avoid reimplementation for each sub catalogue*/
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
	//   ::testing::GTEST_FLAG(filter) = "diggireplay_tests.*";
    //RUN_ALL_TESTS();
    // ::testing::GTEST_FLAG(filter) = "tamperprooflogtests.*";
    // ::testing::GTEST_FLAG(filter) = "bigtest_storageinfrastructure.*";
    //::testing::GTEST_FLAG(filter) = "bigtest_networkinfrastructure.*";
    //::testing::GTEST_FLAG(filter) = "networkmanagertests.simple_server_test";
    //::testing::GTEST_FLAG(filter) = "storageservertests.tls_setup";
    return RUN_ALL_TESTS();
}