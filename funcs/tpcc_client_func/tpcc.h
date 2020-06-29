#ifndef TPCC_H
#define TPCC_H

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include "DiggiAssert.h"
#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <inttypes.h>
#include "storage/DBClient.h"
typedef enum TransactionName
{
    DELIVERY,
    NEW_ORDER,
    ORDER_STATUS,
    PAYMENT,
    STOCK_LEVEL
} TransactionName;

#define TPCC_MONEY_DECIMALS 2

//  Item constants
#define TPCC_NUM_ITEMS 100000
#define TPCC_MIN_IM 1
#define TPCC_MAX_IM 10000
#define TPCC_MIN_PRICE 1.00
#define TPCC_MAX_PRICE 100.00
#define TPCC_MIN_I_NAME 14
#define TPCC_MAX_I_NAME 24
#define TPCC_MIN_I_DATA 26
#define TPCC_MAX_I_DATA 50

//  Warehouse constants
#define TPCC_MIN_TAX 0
#define TPCC_MAX_TAX 0.2000
#define TPCC_TAX_DECIMALS 4
#define TPCC_INITIAL_W_YTD 300000.00
#define TPCC_MIN_NAME 6
#define TPCC_MAX_NAME 10
#define TPCC_MIN_STREET 10
#define TPCC_MAX_STREET 20
#define TPCC_MIN_CITY 10
#define TPCC_MAX_CITY 20
#define TPCC_STATE 2
#define TPCC_ZIP_LENGTH 9
#define TPCC_ZIP_SUFFIX "11111"

//  Stock constants
#define TPCC_MIN_QUANTITY 10
#define TPCC_MAX_QUANTITY 100
#define TPCC_DIST 24
#define TPCC_STOCK_PER_WAREHOUSE 100000

//  District constants
#define TPCC_DISTRICTS_PER_WAREHOUSE 10
#define TPCC_INITIAL_D_YTD 30000.00 //  different from Warehouse
#define TPCC_INITIAL_NEXT_O_ID 3001

//  Customer constants
#define TPCC_CUSTOMERS_PER_DISTRICT 3000
#define TPCC_INITIAL_CREDIT_LIM 50000.00
#define TPCC_MIN_DISCOUNT 0.0000
#define TPCC_MAX_DISCOUNT 0.5000
#define TPCC_DISCOUNT_DECIMALS 4
#define TPCC_INITIAL_BALANCE -10.00
#define TPCC_INITIAL_YTD_PAYMENT 10.00
#define TPCC_INITIAL_PAYMENT_CNT 1
#define TPCC_INITIAL_DELIVERY_CNT 0
#define TPCC_MIN_FIRST 6
#define TPCC_MAX_FIRST 10
#define TPCC_MIDDLE "OE"
#define TPCC_PHONE 16
#define TPCC_MIN_C_DATA 300
#define TPCC_MAX_C_DATA 500
#define TPCC_GOOD_CREDIT "GC"
#define TPCC_BAD_CREDIT "BC"

//  Order constants
#define TPCC_MIN_CARRIER_ID 1
#define TPCC_MAX_CARRIER_ID 10

//  HACK: This is not strictly correct, but it works
#define TPCC_NULL_CARRIER_ID 0L

//  o_id < than this value, carrier != null, >= ->carrier == null
#define TPCC_NULL_CARRIER_LOWER_BOUND 2101
#define TPCC_MIN_OL_CNT 5
#define TPCC_MAX_OL_CNT 15
#define TPCC_INITIAL_ALL_LOCAL 1
#define TPCC_INITIAL_ORDERS_PER_DISTRICT 3000

//  Used to generate new order transactions
#define TPCC_MAX_OL_QUANTITY 10

//  Order line constants
#define TPCC_INITIAL_QUANTITY 5
#define TPCC_MIN_AMOUNT 0.01

//  History constants
#define TPCC_MIN_DATA 12
#define TPCC_MAX_DATA 24
#define TPCC_INITIAL_AMOUNT 10.00

//  New order constants
#define TPCC_INITIAL_NEW_ORDERS_PER_DISTRICT 900

//  TPC - C 2.4.3.4 (page 31) says this must be displayed when new order rolls back.
#define TPCC_INVALID_ITEM_MESSAGE "Item number is not valid"

//  Used to generate stock level transactions
#define TPCC_MIN_STOCK_LEVEL_THRESHOLD 10
#define TPCC_MAX_STOCK_LEVEL_THRESHOLD 20

//  Used to generate payment transactions
#define TPCC_MIN_PAYMENT 1.0
#define TPCC_MAX_PAYMENT 5000.0

//  Indicates "brand" items and stock in i_data and s_data.
#define TPCC_ORIGINAL_STRING "ORIGINAL"

// Table Names
#define TPCC_TABLENAME_ITEM "ITEM"
#define TPCC_TABLENAME_WAREHOUSE "WAREHOUSE"
#define TPCC_TABLENAME_DISTRICT "DISTRICT"
#define TPCC_TABLENAME_CUSTOMER "CUSTOMER"
#define TPCC_TABLENAME_STOCK "STOCK"
#define TPCC_TABLENAME_ORDERS "ORDERS"
#define TPCC_TABLENAME_NEW_ORDER "NEW_ORDER"
#define TPCC_TABLENAME_ORDER_LINE "ORDER_LINE"
#define TPCC_TABLENAME_HISTORY "HISTORY"
static const std::string tpcc_drop_tables = "DROP TABLE IF EXISTS ORDER_LINE;"
                                            "DROP TABLE IF EXISTS NEW_ORDER;"
                                            "DROP TABLE IF EXISTS ORDERS;"
                                            "DROP TABLE IF EXISTS STOCK;"
                                            "DROP TABLE IF EXISTS HISTORY;"
                                            "DROP TABLE IF EXISTS CUSTOMER;"
                                            "DROP TABLE IF EXISTS ITEM;"
                                            "DROP TABLE IF EXISTS DISTRICT;"
                                            "DROP TABLE IF EXISTS WAREHOUSE;";

static const std::string tpcc_create_tables = "CREATE TABLE WAREHOUSE ("
                                              "W_ID SMALLINT DEFAULT '0' NOT NULL,"
                                              "W_NAME VARCHAR(16) DEFAULT NULL,"
                                              "W_STREET_1 VARCHAR(32) DEFAULT NULL,"
                                              "W_STREET_2 VARCHAR(32) DEFAULT NULL,"
                                              "W_CITY VARCHAR(32) DEFAULT NULL,"
                                              "W_STATE VARCHAR(2) DEFAULT NULL,"
                                              "W_ZIP VARCHAR(9) DEFAULT NULL,"
                                              "W_TAX FLOAT DEFAULT NULL,"
                                              "W_YTD FLOAT DEFAULT NULL,"
                                              "CONSTRAINT W_PK_ARRAY PRIMARY KEY (W_ID)"
                                              ");"
                                              "CREATE TABLE DISTRICT ("
                                              "D_ID TINYINT DEFAULT '0' NOT NULL,"
                                              "D_W_ID SMALLINT DEFAULT '0' NOT NULL REFERENCES WAREHOUSE (W_ID),"
                                              "D_NAME VARCHAR(16) DEFAULT NULL,"
                                              "D_STREET_1 VARCHAR(32) DEFAULT NULL,"
                                              "D_STREET_2 VARCHAR(32) DEFAULT NULL,"
                                              "D_CITY VARCHAR(32) DEFAULT NULL,"
                                              "D_STATE VARCHAR(2) DEFAULT NULL,"
                                              "D_ZIP VARCHAR(9) DEFAULT NULL,"
                                              "D_TAX FLOAT DEFAULT NULL,"
                                              "D_YTD FLOAT DEFAULT NULL,"
                                              "D_NEXT_O_ID INT DEFAULT NULL,"
                                              "PRIMARY KEY (D_W_ID,D_ID)"
                                              ");"
                                              "CREATE TABLE ITEM ("
                                              "I_ID INTEGER DEFAULT '0' NOT NULL,"
                                              "I_IM_ID INTEGER DEFAULT NULL,"
                                              "I_NAME VARCHAR(32) DEFAULT NULL,"
                                              "I_PRICE FLOAT DEFAULT NULL,"
                                              "I_DATA VARCHAR(64) DEFAULT NULL,"
                                              "CONSTRAINT I_PK_ARRAY PRIMARY KEY (I_ID)"
                                              ");"
                                              "CREATE TABLE CUSTOMER ("
                                              "C_ID INTEGER DEFAULT '0' NOT NULL,"
                                              "C_D_ID TINYINT DEFAULT '0' NOT NULL,"
                                              "C_W_ID SMALLINT DEFAULT '0' NOT NULL,"
                                              "C_FIRST VARCHAR(32) DEFAULT NULL,"
                                              "C_MIDDLE VARCHAR(2) DEFAULT NULL,"
                                              "C_LAST VARCHAR(32) DEFAULT NULL,"
                                              "C_STREET_1 VARCHAR(32) DEFAULT NULL,"
                                              "C_STREET_2 VARCHAR(32) DEFAULT NULL,"
                                              "C_CITY VARCHAR(32) DEFAULT NULL,"
                                              "C_STATE VARCHAR(2) DEFAULT NULL,"
                                              "C_ZIP VARCHAR(9) DEFAULT NULL,"
                                              "C_PHONE VARCHAR(32) DEFAULT NULL,"
                                              "C_SINCE TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,"
                                              "C_CREDIT VARCHAR(2) DEFAULT NULL,"
                                              "C_CREDIT_LIM FLOAT DEFAULT NULL,"
                                              "C_DISCOUNT FLOAT DEFAULT NULL,"
                                              "C_BALANCE FLOAT DEFAULT NULL,"
                                              "C_YTD_PAYMENT FLOAT DEFAULT NULL,"
                                              "C_PAYMENT_CNT INTEGER DEFAULT NULL,"
                                              "C_DELIVERY_CNT INTEGER DEFAULT NULL,"
                                              "C_DATA VARCHAR(500),"
                                              "PRIMARY KEY (C_W_ID,C_D_ID,C_ID),"
                                              "UNIQUE (C_W_ID,C_D_ID,C_LAST,C_FIRST),"
                                              "CONSTRAINT C_FKEY_D FOREIGN KEY (C_D_ID, C_W_ID) REFERENCES DISTRICT (D_ID, D_W_ID)"
                                              ");"
                                              "CREATE INDEX IDX_CUSTOMER ON CUSTOMER (C_W_ID,C_D_ID,C_LAST);"
                                              "CREATE TABLE HISTORY ("
                                              "H_C_ID INTEGER DEFAULT NULL,"
                                              "H_C_D_ID TINYINT DEFAULT NULL,"
                                              "H_C_W_ID SMALLINT DEFAULT NULL,"
                                              "H_D_ID TINYINT DEFAULT NULL,"
                                              "H_W_ID SMALLINT DEFAULT '0' NOT NULL,"
                                              "H_DATE TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,"
                                              "H_AMOUNT FLOAT DEFAULT NULL,"
                                              "H_DATA VARCHAR(32) DEFAULT NULL,"
                                              "CONSTRAINT H_FKEY_C FOREIGN KEY (H_C_ID, H_C_D_ID, H_C_W_ID) REFERENCES CUSTOMER (C_ID, C_D_ID, C_W_ID),"
                                              "CONSTRAINT H_FKEY_D FOREIGN KEY (H_D_ID, H_W_ID) REFERENCES DISTRICT (D_ID, D_W_ID)"
                                              ");"
                                              "CREATE TABLE STOCK ("
                                              "S_I_ID INTEGER DEFAULT '0' NOT NULL REFERENCES ITEM (I_ID),"
                                              "S_W_ID SMALLINT DEFAULT '0 ' NOT NULL REFERENCES WAREHOUSE (W_ID),"
                                              "S_QUANTITY INTEGER DEFAULT '0' NOT NULL,"
                                              "S_DIST_01 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_02 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_03 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_04 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_05 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_06 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_07 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_08 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_09 VARCHAR(32) DEFAULT NULL,"
                                              "S_DIST_10 VARCHAR(32) DEFAULT NULL,"
                                              "S_YTD INTEGER DEFAULT NULL,"
                                              "S_ORDER_CNT INTEGER DEFAULT NULL,"
                                              "S_REMOTE_CNT INTEGER DEFAULT NULL,"
                                              "S_DATA VARCHAR(64) DEFAULT NULL,"
                                              "PRIMARY KEY (S_W_ID,S_I_ID)"
                                              ");"
                                              "CREATE TABLE ORDERS ("
                                              "O_ID INTEGER DEFAULT '0' NOT NULL,"
                                              "O_C_ID INTEGER DEFAULT NULL,"
                                              "O_D_ID TINYINT DEFAULT '0' NOT NULL,"
                                              "O_W_ID SMALLINT DEFAULT '0' NOT NULL,"
                                              "O_ENTRY_D TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,"
                                              "O_CARRIER_ID INTEGER DEFAULT NULL,"
                                              "O_OL_CNT INTEGER DEFAULT NULL,"
                                              "O_ALL_LOCAL INTEGER DEFAULT NULL,"
                                              "PRIMARY KEY (O_W_ID,O_D_ID,O_ID),"
                                              "UNIQUE (O_W_ID,O_D_ID,O_C_ID,O_ID),"
                                              "CONSTRAINT O_FKEY_C FOREIGN KEY (O_C_ID, O_D_ID, O_W_ID) REFERENCES CUSTOMER (C_ID, C_D_ID, C_W_ID)"
                                              ");"
                                              "CREATE INDEX IDX_ORDERS ON ORDERS (O_W_ID,O_D_ID,O_C_ID);"
                                              "CREATE TABLE NEW_ORDER ("
                                              "NO_O_ID INTEGER DEFAULT '0' NOT NULL,"
                                              "NO_D_ID TINYINT DEFAULT '0' NOT NULL,"
                                              "NO_W_ID SMALLINT DEFAULT '0' NOT NULL,"
                                              "CONSTRAINT NO_PK_TREE PRIMARY KEY (NO_D_ID,NO_W_ID,NO_O_ID),"
                                              "CONSTRAINT NO_FKEY_O FOREIGN KEY (NO_O_ID, NO_D_ID, NO_W_ID) REFERENCES ORDERS (O_ID, O_D_ID, O_W_ID)"
                                              ");"
                                              "CREATE TABLE ORDER_LINE ("
                                              "OL_O_ID INTEGER DEFAULT '0' NOT NULL,"
                                              "OL_D_ID TINYINT DEFAULT '0' NOT NULL,"
                                              "OL_W_ID SMALLINT DEFAULT '0' NOT NULL,"
                                              "OL_NUMBER INTEGER DEFAULT '0' NOT NULL,"
                                              "OL_I_ID INTEGER DEFAULT NULL,"
                                              "OL_SUPPLY_W_ID SMALLINT DEFAULT NULL,"
                                              "OL_DELIVERY_D TIMESTAMP DEFAULT NULL,"
                                              "OL_QUANTITY INTEGER DEFAULT NULL,"
                                              "OL_AMOUNT FLOAT DEFAULT NULL,"
                                              "OL_DIST_INFO VARCHAR(32) DEFAULT NULL,"
                                              "PRIMARY KEY (OL_W_ID,OL_D_ID,OL_O_ID,OL_NUMBER),"
                                              "CONSTRAINT OL_FKEY_O FOREIGN KEY (OL_O_ID, OL_D_ID, OL_W_ID) REFERENCES ORDERS (O_ID, O_D_ID, O_W_ID),"
                                              "CONSTRAINT OL_FKEY_S FOREIGN KEY (OL_I_ID, OL_SUPPLY_W_ID) REFERENCES STOCK (S_I_ID, S_W_ID)"
                                              ");" /*"--CREATE INDEX IDX_ORDER_LINE_3COL ON ORDER_LINE (OL_W_ID,OL_D_ID,OL_O_ID);"\
"--CREATE INDEX IDX_ORDER_LINE_2COL ON ORDER_LINE (OL_W_ID,OL_D_ID);"\*/
                                              "CREATE INDEX IDX_ORDER_LINE_TREE ON ORDER_LINE (OL_W_ID,OL_D_ID,OL_O_ID);";

static std::map<std::string, std::map<std::string, const char *>> transaction_queries = {
    {"DELIVERY", {
                     {"getNewOrder", "SELECT NO_O_ID FROM NEW_ORDER WHERE NO_D_ID = %" PRIu64 " AND NO_W_ID = %" PRIu64 " AND NO_O_ID > -1 LIMIT 1"}, {"deleteNewOrder", "DELETE FROM NEW_ORDER WHERE NO_D_ID = %" PRIu64 " AND NO_W_ID = %" PRIu64 " AND NO_O_ID = %" PRIu64 ""}, {"getCId", "SELECT O_C_ID FROM ORDERS WHERE O_ID = %" PRIu64 " AND O_D_ID = %" PRIu64 " AND O_W_ID = %" PRIu64 ""}, // no_o_id, d_id, w_id
                     {"updateOrders", "UPDATE ORDERS SET O_CARRIER_ID = %" PRIu64 " WHERE O_ID = %" PRIu64 " AND O_D_ID = %" PRIu64 " AND O_W_ID = %" PRIu64 ""},                                                                                                                                                                                                                                      // o_carrier_id, no_o_id, d_id, w_id
                     {"updateOrderLine", "UPDATE ORDER_LINE SET OL_DELIVERY_D = '%s' WHERE OL_O_ID = %" PRIu64 " AND OL_D_ID = %" PRIu64 " AND OL_W_ID = %" PRIu64 ""},                                                                                                                                                                                                                                // o_entry_d, no_o_id, d_id, w_id
                     {"sumOLAmount", "SELECT SUM(OL_AMOUNT) FROM ORDER_LINE WHERE OL_O_ID = %" PRIu64 " AND OL_D_ID = %" PRIu64 " AND OL_W_ID = %" PRIu64 ""},                                                                                                                                                                                                                                         // no_o_id, d_id, w_id
                     {"updateCustomer", "UPDATE CUSTOMER SET C_BALANCE = C_BALANCE + %lf WHERE C_ID = %" PRIu64 " AND C_D_ID = %" PRIu64 " AND C_W_ID = %" PRIu64 ""}                                                                                                                                                                                                                                  // ol_total, c_id, d_id, w_id
                 }},
    {"NEW_ORDER", {
                      {"getWarehouseTaxRate", "SELECT W_TAX FROM WAREHOUSE WHERE W_ID = %" PRIu64 ""},
                      {"getDistrict", "SELECT D_TAX, D_NEXT_O_ID FROM DISTRICT WHERE D_ID = %" PRIu64 " AND D_W_ID = %" PRIu64 ""},
                      {"incrementNextOrderId", "UPDATE DISTRICT SET D_NEXT_O_ID = %" PRIu64 " WHERE D_ID = %" PRIu64 " AND D_W_ID = %" PRIu64 ""},
                      {"getCustomer", "SELECT C_DISCOUNT, C_LAST, C_CREDIT FROM CUSTOMER WHERE C_W_ID = %" PRIu64 " AND C_D_ID = %" PRIu64 " AND C_ID = %" PRIu64 ""},
                      {"createOrder", "INSERT INTO ORDERS (O_ID, O_D_ID, O_W_ID, O_C_ID, O_ENTRY_D, O_CARRIER_ID, O_OL_CNT, O_ALL_LOCAL) VALUES (%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", '%s', %" PRIu64 ", %" PRIu64 ", %" PRIu64 ")"},
                      {"createNewOrder", "INSERT INTO NEW_ORDER (NO_O_ID, NO_D_ID, NO_W_ID) VALUES (%" PRIu64 ", %" PRIu64 ", %" PRIu64 ")"},
                      {"getItemInfo", "SELECT I_PRICE, I_NAME, I_DATA FROM ITEM WHERE I_ID = %" PRIu64 ""},
                      {"getStockInfo", "SELECT S_QUANTITY, S_DATA, S_YTD, S_ORDER_CNT, S_REMOTE_CNT, S_DIST_%02d FROM STOCK WHERE S_I_ID = %" PRIu64 " AND S_W_ID = %" PRIu64 ""},
                      {"updateStock", "UPDATE STOCK SET S_QUANTITY = %" PRIu64 ", S_YTD = %" PRIu64 ", S_ORDER_CNT = %" PRIu64 ", S_REMOTE_CNT = %" PRIu64 " WHERE S_I_ID = %" PRIu64 " AND S_W_ID = %" PRIu64 ""},
                      {"createOrderLine", "INSERT INTO ORDER_LINE (OL_O_ID, OL_D_ID, OL_W_ID, OL_NUMBER, OL_I_ID, OL_SUPPLY_W_ID, OL_DELIVERY_D, OL_QUANTITY, OL_AMOUNT, OL_DIST_INFO) VALUES (%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", '%s', %" PRIu64 ", %lf, '%s')"},
                  }},
    {"ORDER_STATUS", {{"getCustomerByCustomerId", "SELECT C_ID, C_FIRST, C_MIDDLE, C_LAST, C_BALANCE FROM CUSTOMER WHERE C_W_ID = %" PRIu64 " AND C_D_ID = %" PRIu64 " AND C_ID = %" PRIu64 ""}, {"getCustomersByLastName", "SELECT C_ID, C_FIRST, C_MIDDLE, C_LAST, C_BALANCE FROM CUSTOMER WHERE C_W_ID = %" PRIu64 " AND C_D_ID = %" PRIu64 " AND C_LAST = '%s' ORDER BY C_FIRST"}, {"getLastOrder", "SELECT O_ID, O_CARRIER_ID, O_ENTRY_D FROM ORDERS WHERE O_W_ID = %" PRIu64 " AND O_D_ID = %" PRIu64 " AND O_C_ID = %" PRIu64 " ORDER BY O_ID DESC LIMIT 1"}, {"getOrderLines", "SELECT OL_SUPPLY_W_ID, OL_I_ID, OL_QUANTITY, OL_AMOUNT, OL_DELIVERY_D FROM ORDER_LINE WHERE OL_W_ID = %" PRIu64 " AND OL_D_ID = %" PRIu64 " AND OL_O_ID = %" PRIu64 ""}}},
    {"PAYMENT", {{"getWarehouse", "SELECT W_NAME, W_STREET_1, W_STREET_2, W_CITY, W_STATE, W_ZIP FROM WAREHOUSE WHERE W_ID = %" PRIu64 ""}, {"updateWarehouseBalance", "UPDATE WAREHOUSE SET W_YTD = W_YTD + %lf WHERE W_ID = %" PRIu64 ""}, {"getDistrict", "SELECT D_NAME, D_STREET_1, D_STREET_2, D_CITY, D_STATE, D_ZIP FROM DISTRICT WHERE D_W_ID = %" PRIu64 " AND D_ID = %" PRIu64 ""}, {"updateDistrictBalance", "UPDATE DISTRICT SET D_YTD = D_YTD + %lf WHERE D_W_ID  = %" PRIu64 " AND D_ID = %" PRIu64 ""}, {"getCustomerByCustomerId", "SELECT C_ID, C_FIRST, C_MIDDLE, C_LAST, C_STREET_1, C_STREET_2, C_CITY, C_STATE, C_ZIP, C_PHONE, C_SINCE, C_CREDIT, C_CREDIT_LIM, C_DISCOUNT, C_BALANCE, C_YTD_PAYMENT, C_PAYMENT_CNT, C_DATA FROM CUSTOMER WHERE C_W_ID = %" PRIu64 " AND C_D_ID = %" PRIu64 " AND C_ID = %" PRIu64 ""}, {"getCustomersByLastName", "SELECT C_ID, C_FIRST, C_MIDDLE, C_LAST, C_STREET_1, C_STREET_2, C_CITY, C_STATE, C_ZIP, C_PHONE, C_SINCE, C_CREDIT, C_CREDIT_LIM, C_DISCOUNT, C_BALANCE, C_YTD_PAYMENT, C_PAYMENT_CNT, C_DATA FROM CUSTOMER WHERE C_W_ID = %" PRIu64 " AND C_D_ID = %" PRIu64 " AND C_LAST = '%s' ORDER BY C_FIRST"}, {"updateBCCustomer", "UPDATE CUSTOMER SET C_BALANCE = %lf, C_YTD_PAYMENT = %lf, C_PAYMENT_CNT = %" PRIu64 ", C_DATA = '%s' WHERE C_W_ID = %" PRIu64 " AND C_D_ID = %" PRIu64 " AND C_ID = %" PRIu64 ""}, {"updateGCCustomer", "UPDATE CUSTOMER SET C_BALANCE = %lf, C_YTD_PAYMENT = %lf, C_PAYMENT_CNT = %" PRIu64 " WHERE C_W_ID = %" PRIu64 " AND C_D_ID = %" PRIu64 " AND C_ID = %" PRIu64 ""}, {"insertHistory", "INSERT INTO HISTORY VALUES (%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", '%s', %lf, '%s')"}}},
    {"STOCK_LEVEL", {{"getOId", "SELECT D_NEXT_O_ID FROM DISTRICT WHERE D_W_ID = %" PRIu64 " AND D_ID = %" PRIu64 ""}, {"getStockCount", "SELECT COUNT(DISTINCT(OL_I_ID)) FROM ORDER_LINE, STOCK "
                                                                                                                                         "WHERE OL_W_ID = %" PRIu64 " "
                                                                                                                                         "AND OL_D_ID = %" PRIu64 " "
                                                                                                                                         "AND OL_O_ID < %" PRIu64 " "
                                                                                                                                         "AND OL_O_ID >= %" PRIu64 " "
                                                                                                                                         "AND S_W_ID = %" PRIu64 " "
                                                                                                                                         "AND S_I_ID = OL_I_ID "
                                                                                                                                         "AND S_QUANTITY < %" PRIu64 " "}}}};

class Item
{
public:
    Item(uint64_t i_id, uint64_t i_im_id, std::string i_name, double i_price, std::string i_data) : i_id(i_id),
                                                                                                    i_im_id(i_im_id),
                                                                                                    i_name(i_name),
                                                                                                    i_price(i_price),
                                                                                                    i_data(i_data) {}

    uint64_t i_id;
    uint64_t i_im_id;
    std::string i_name;
    double i_price;
    std::string i_data;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << i_id << ",";
        os << i_im_id << ",";
        os << "'" << i_name << "',";
        os << i_price << ",";
        os << "'" << i_data << "'";
        return os.str();
    }
};

class Address
{
public:
    Address(std::string name,
            std::string street1,
            std::string street2,
            std::string city,
            std::string state,
            std::string zip) : name(name),
                               street1(street1),
                               street2(street2),
                               city(city),
                               state(state),
                               zip(zip)
    {
    }
    std::string name;
    std::string street1;
    std::string street2;
    std::string city;
    std::string state;
    std::string zip;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << "'" << name << "',";
        os << "'" << street1 << "',";
        os << "'" << street2 << "',";
        os << "'" << city << "',";
        os << "'" << state << "',";
        os << "'" << zip << "'";
        return os.str();
    }
};
class Warehouse
{
public:
    Warehouse(uint64_t w_id, Address w_address, double w_tax, double w_ytd) : w_id(w_id),
                                                                              w_address(w_address),
                                                                              w_tax(w_tax),
                                                                              w_ytd(w_ytd)
    {
    }
    uint64_t w_id;
    Address w_address;
    double w_tax;
    double w_ytd;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << w_id << ",";
        os << w_address.getInsertStr() << ",";
        os << w_tax << ",";
        os << w_ytd;
        return os.str();
    }
};

class Stock
{
public:
    Stock(uint64_t s_i_id,
          uint64_t s_w_id,
          uint64_t s_quantity,
          std::vector<std::string> s_dists,
          uint64_t s_ytd,
          uint64_t s_order_cnt,
          uint64_t s_remote_cnt,
          std::string s_data) : s_i_id(s_i_id),
                                s_w_id(s_w_id),
                                s_quantity(s_quantity),
                                s_dists(s_dists),
                                s_ytd(s_ytd),
                                s_order_cnt(s_order_cnt),
                                s_remote_cnt(s_remote_cnt),
                                s_data(s_data)
    {
    }
    uint64_t s_i_id;
    uint64_t s_w_id;
    uint64_t s_quantity;
    std::vector<std::string> s_dists;
    uint64_t s_ytd;
    uint64_t s_order_cnt;
    uint64_t s_remote_cnt;
    std::string s_data;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << s_i_id << ",";
        os << s_w_id << ",";
        os << s_quantity << ",";
        for (auto dist : s_dists)
        {
            os << "'" << dist << "',";
        }
        os << s_ytd << ",";
        os << s_order_cnt << ",";
        os << s_remote_cnt << ",";
        os << "'" << s_data << "'";
        return os.str();
    }
};
class OpParam
{
public:
    double h_amount;
    uint64_t w_id;
    uint64_t d_id;
    std::string h_date;
    std::string ol_delivery_d;
    std::string o_entry_d;
    uint64_t o_carrier_id;
    uint64_t c_w_id;
    uint64_t c_d_id;
    uint64_t c_id;
    std::vector<uint64_t> i_ids;
    std::vector<uint64_t> i_w_ids;
    std::vector<uint64_t> i_qtys;
    std::string c_last;
    uint64_t threshold;
};

class District
{
public:
    District(
        uint64_t d_id,
        uint64_t d_w_id,
        Address d_address,
        double d_tax,
        double d_ytd,
        uint64_t d_next_o_id) : d_id(d_id),
                                d_w_id(d_w_id),
                                d_address(d_address),
                                d_tax(d_tax),
                                d_ytd(d_ytd),
                                d_next_o_id(d_next_o_id)
    {
    }

    uint64_t d_id;
    uint64_t d_w_id;
    Address d_address;
    double d_tax;
    double d_ytd;
    uint64_t d_next_o_id;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << d_id << ",";
        os << d_w_id << ",";
        os << d_address.getInsertStr() << ",";
        os << d_tax << ",";
        os << d_ytd << ",";
        os << d_next_o_id;

        return os.str();
    }
};
class Order
{
public:
    Order(uint64_t o_id,
          uint64_t o_c_id,
          uint64_t o_d_id,
          uint64_t o_w_id,
          std::string o_entry_d,
          uint64_t o_carrier_id,
          uint64_t o_ol_cnt,
          uint64_t o_all_local) : o_id(o_id),
                                  o_c_id(o_c_id),
                                  o_d_id(o_d_id), o_w_id(o_w_id),
                                  o_entry_d(o_entry_d),
                                  o_carrier_id(o_carrier_id),
                                  o_ol_cnt(o_ol_cnt),
                                  o_all_local(o_all_local)
    {
    }

    uint64_t o_id;
    uint64_t o_c_id;
    uint64_t o_d_id;
    uint64_t o_w_id;
    std::string o_entry_d;
    uint64_t o_carrier_id;
    uint64_t o_ol_cnt;
    uint64_t o_all_local;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << o_id << ",";
        os << o_c_id << ",";
        os << o_d_id << ",";
        os << o_w_id << ",";
        os << "'" << o_entry_d << "',";
        os << o_carrier_id << ",";
        os << o_ol_cnt << ",";
        os << o_all_local;
        return os.str();
    }
};
class OrderLine
{
public:
    OrderLine(uint64_t ol_o_id,
              uint64_t ol_d_id,
              uint64_t ol_w_id,
              uint64_t ol_number,
              uint64_t ol_i_id,
              uint64_t ol_supply_w_id,
              std::string ol_delivery_d,
              uint64_t ol_quantity,
              double ol_amount,
              std::string ol_dist_info) : ol_o_id(ol_o_id),
                                          ol_d_id(ol_d_id),
                                          ol_w_id(ol_w_id),
                                          ol_number(ol_number),
                                          ol_i_id(ol_i_id),
                                          ol_supply_w_id(ol_supply_w_id),
                                          ol_delivery_d(ol_delivery_d),
                                          ol_quantity(ol_quantity),
                                          ol_amount(ol_amount),
                                          ol_dist_info(ol_dist_info)
    {
    }
    uint64_t ol_o_id;
    uint64_t ol_d_id;
    uint64_t ol_w_id;
    uint64_t ol_number;
    uint64_t ol_i_id;
    uint64_t ol_supply_w_id;
    std::string ol_delivery_d;
    uint64_t ol_quantity;
    double ol_amount;
    std::string ol_dist_info;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << ol_o_id << ",";
        os << ol_d_id << ",";
        os << ol_w_id << ",";
        os << ol_number << ",";
        os << ol_i_id << ",";
        os << ol_supply_w_id << ",";
        os << "'" << ol_delivery_d << "',";
        os << ol_quantity << ",";
        os << ol_amount << ",";
        os << "'" << ol_dist_info << "'";
        return os.str();
    }
};

class NewOrder
{
public:
    uint64_t o_id;
    uint64_t d_id;
    uint64_t w_id;
    NewOrder(uint64_t o_id, uint64_t d_id, uint64_t w_id) : o_id(o_id), d_id(d_id), w_id(w_id)
    {
    }
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << o_id << ",";
        os << d_id << ",";
        os << w_id;
        return os.str();
    }
};
class History
{
public:
    History(uint64_t h_c_id,
            uint64_t h_c_d_id,
            uint64_t h_c_w_id,
            uint64_t h_d_id,
            uint64_t h_w_id,
            std::string h_date,
            double h_amount,
            std::string h_data) : h_c_id(h_c_id),
                                  h_c_d_id(h_c_d_id),
                                  h_c_w_id(h_c_w_id),
                                  h_d_id(h_d_id),
                                  h_w_id(h_w_id),
                                  h_date(h_date),
                                  h_amount(h_amount),
                                  h_data(h_data)
    {
    }
    uint64_t h_c_id;
    uint64_t h_c_d_id;
    uint64_t h_c_w_id;
    uint64_t h_d_id;
    uint64_t h_w_id;
    std::string h_date;
    double h_amount;
    std::string h_data;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << h_c_id << ",";
        os << h_c_d_id << ",";
        os << h_c_w_id << ",";
        os << h_d_id << ",";
        os << h_w_id << ",";
        os << "'" << h_date << "',";
        os << h_amount << ",";
        os << "'" << h_data << "'";
        return os.str();
    }
};
class Customer
{
public:
    Customer(
        uint64_t c_id,
        uint64_t c_d_id,
        uint64_t c_w_id,
        std::string c_first,
        std::string c_middle,
        std::string c_last,
        std::string c_street1,
        std::string c_street2,
        std::string c_city,
        std::string c_state,
        std::string c_zip,
        std::string c_phone,
        std::string c_since,
        std::string c_credit,
        double c_credit_lim,
        double c_discount,
        double c_balance,
        double c_ytd_payment,
        uint64_t c_payment_cnt,
        uint64_t c_delivery_cnt,
        std::string c_data) : c_id(c_id),
                              c_d_id(c_d_id),
                              c_w_id(c_w_id),
                              c_first(c_first),
                              c_middle(c_middle),
                              c_last(c_last),
                              c_street1(c_street1),
                              c_street2(c_street2),
                              c_city(c_city),
                              c_state(c_state),
                              c_zip(c_zip),
                              c_phone(c_phone),
                              c_since(c_since),
                              c_credit(c_credit),
                              c_credit_lim(c_credit_lim),
                              c_discount(c_discount),
                              c_balance(c_balance),
                              c_ytd_payment(c_ytd_payment),
                              c_payment_cnt(c_payment_cnt),
                              c_delivery_cnt(c_delivery_cnt),
                              c_data(c_data)
    {
    }

    uint64_t c_id;
    uint64_t c_d_id;
    uint64_t c_w_id;
    std::string c_first;
    std::string c_middle;
    std::string c_last;
    std::string c_street1;
    std::string c_street2;
    std::string c_city;
    std::string c_state;
    std::string c_zip;
    std::string c_phone;
    std::string c_since;
    std::string c_credit;
    double c_credit_lim;
    double c_discount;
    double c_balance;
    double c_ytd_payment;
    uint64_t c_payment_cnt;
    uint64_t c_delivery_cnt;
    std::string c_data;
    std::string getInsertStr()
    {
        std::ostringstream os;
        os << c_id << ",";
        os << c_d_id << ",";
        os << c_w_id << ",";
        os << "'" << c_first << "',";
        os << "'" << c_middle << "',";
        os << "'" << c_last << "',";
        os << "'" << c_street1 << "',";
        os << "'" << c_street2 << "',";
        os << "'" << c_city << "',";
        os << "'" << c_state << "',";
        os << "'" << c_zip << "',";
        os << "'" << c_phone << "',";
        os << "'" << c_since << "',";
        os << "'" << c_credit << "',";
        os << c_credit_lim << ",";
        os << c_discount << ",";
        os << c_balance << ",";
        os << c_ytd_payment << ",";
        os << c_payment_cnt << ",";
        os << c_delivery_cnt << ",";
        os << "'" << c_data << "'";
        return os.str();
    }
};
class DBDriver
{
    IDBClient *client;

public:
    DBDriver(IDBClient *client) : client(client)
    {
    }

    void loadConfig(std::map<std::string, const char *> config, bool load);
    template <class T>
    void loadTuples(std::string tableName, std::vector<T> tuple)
    {
        std::vector<std::string> statements = {};
        for (auto item : tuple)
        {
            auto sqlitems = item.getInsertStr();
            auto sql = "INSERT INTO " + tableName + " VALUES(" + sqlitems + ");";
            statements.push_back(sql);
        }
        this->client->executemany(statements);
    }
    int executeTransaction(TransactionName txn, OpParam param);
    void loadFinish(void);
    int doDelivery(OpParam param);
    int doNewOrder(OpParam param);
    int doOrderStatus(OpParam param);
    int doPayment(OpParam param);
    int doStockLevel(OpParam param);
};

class ScaleParameters
{
public:
    uint64_t items;
    uint64_t warehouses;
    uint64_t starting_warehouse;
    uint64_t districts_per_warehouse;
    uint64_t customers_per_district;
    uint64_t new_orders_per_district;
    uint64_t ending_warehouse;
    ScaleParameters(
        uint64_t items,
        uint64_t warehouses,
        uint64_t districts_per_warehouse,
        uint64_t customers_per_district,
        uint64_t new_orders_per_district) : items(items),
                                            warehouses(warehouses),
                                            starting_warehouse(1),
                                            districts_per_warehouse(districts_per_warehouse),
                                            customers_per_district(customers_per_district),
                                            new_orders_per_district(new_orders_per_district),
                                            ending_warehouse(warehouses + starting_warehouse - 1)
    {
        DIGGI_ASSERT((1 <= items) && (items <= TPCC_NUM_ITEMS));
        DIGGI_ASSERT(warehouses > 0);
        DIGGI_ASSERT((1 <= districts_per_warehouse) && (districts_per_warehouse <= TPCC_DISTRICTS_PER_WAREHOUSE));
        DIGGI_ASSERT((1 <= customers_per_district) && (customers_per_district <= TPCC_CUSTOMERS_PER_DISTRICT));
        DIGGI_ASSERT((0 <= new_orders_per_district) && (new_orders_per_district <= TPCC_CUSTOMERS_PER_DISTRICT));
        DIGGI_ASSERT(new_orders_per_district <= TPCC_INITIAL_NEW_ORDERS_PER_DISTRICT);
    }
    static ScaleParameters *makeWithScaleFactor(uint64_t warehouses, double scaleFactor);
    static ScaleParameters *makeDefault(uint64_t warehouses);
};

class NURandC
{
public:
    uint64_t cLast;
    uint64_t cId;
    uint64_t orderrLineItemId;
};

class Random
{
    NURandC *nurandVar;
    std::vector<std::string> syllables;

public:
    Random() : nurandVar(nullptr)
    {
        syllables = {
            "BAR",
            "OUGHT",
            "ABLE",
            "PRI",
            "PRES",
            "ESE",
            "ANTI",
            "CALLY",
            "ATION",
            "EING"};
        srand(time(NULL));
    }
    NURandC *makeForLoad();
    void setNURand(NURandC *val);
    uint64_t NURand(uint64_t a, uint64_t x, uint64_t y);
    uint64_t number(uint64_t min, uint64_t max);
    uint64_t numberExcluding(uint64_t min, uint64_t max, uint64_t excluding);
    double fixedPoint(uint64_t decimalplaces, double min, double max);
    std::string datetimeNow();
    std::vector<uint64_t> selectUniqueIds(uint64_t numUnique, uint64_t min, uint64_t max);
    std::string astring(uint64_t minlen, uint64_t maxlen);
    std::string nstring(uint64_t minlen, uint64_t maxlen);
    std::string randomString(uint64_t min, uint64_t max, char base, uint64_t numCharacters);
    std::string makeLastName(uint64_t number);
    std::string makeRandomLastName(uint64_t max_c_id);
};
class Loader
{
    DBDriver *handle;
    ScaleParameters *scaleParameters;
    std::vector<uint64_t> w_ids;
    bool needLoadItems;
    uint64_t batchSize;
    Random *rand;

public:
    Loader(DBDriver *handle, ScaleParameters *scaleParameters, std::vector<uint64_t> w_ids, bool needLoadItems, Random *rand) : handle(handle),
                                                                                                                                scaleParameters(scaleParameters),
                                                                                                                                w_ids(w_ids),
                                                                                                                                needLoadItems(needLoadItems),
                                                                                                                                batchSize(2500),
                                                                                                                                rand(rand) {}

    void execute(void);
    void loadItems(void);
    void loadWarehouse(uint64_t w_id);
    Item generateItem(uint64_t id, bool original);
    Warehouse generateWarehouse(uint64_t w_id);
    District generateDistrict(uint64_t d_w_id, uint64_t d_id, uint64_t d_next_o_id);
    Customer generateCustomer(uint64_t c_w_id, uint64_t c_d_id, uint64_t c_id, bool badCredit);
    Order generateOrder(uint64_t o_w_id, uint64_t o_d_id, uint64_t o_id, uint64_t o_c_id, uint64_t o_ol_cnt, bool newOrder);
    OrderLine generateOrderLine(uint64_t ol_w_id, uint64_t ol_d_id, uint64_t ol_o_id, uint64_t ol_number, uint64_t max_items, bool newOrder);
    Stock generateStock(uint64_t s_w_id, uint64_t s_i_id, bool original);
    History generateHistory(uint64_t h_c_w_id, uint64_t h_c_d_id, uint64_t h_c_id);
    Address generateAddress(void);
    Address generateStreetAddress(void);
    double generateTax(void);
    std::string generateZip(void);
    std::string fillOriginal(std::string data);
};

class Results
{
public:
    std::chrono::system_clock::time_point start;
    std::chrono::system_clock::time_point stop;
    uint64_t txn_id;
    std::map<TransactionName, uint64_t> txn_counters;
    std::map<TransactionName, std::chrono::duration<double>> txn_times;
    std::map<uint64_t, std::tuple<TransactionName, std::chrono::system_clock::time_point>> running;
    std::chrono::system_clock::time_point startBenchmark();
    void stopBenchmark();
    uint64_t startTransaction(TransactionName tp);
    void abortTransaction(uint64_t id);
    void stopTransaction(uint64_t id);
    void append(Results r);
    void tostring();

public:
    Results()
    {
    }
};
class Executor
{
    DBDriver *driver;
    ScaleParameters *param;
    bool stop_on_error;
    Random *rand;

public:
    Executor(DBDriver *driver, ScaleParameters *param, bool stop_on_error, Random *rand) : driver(driver), param(param), stop_on_error(stop_on_error), rand(rand)
    {
    }
    Results execute(std::chrono::duration<double> duration);
    std::tuple<TransactionName, OpParam> doOne();
    OpParam generateNewOrderParams();
    OpParam generatePaymentParams();
    OpParam generateOrderStatusParams();
    OpParam generateDeliveryParams();
    OpParam generateStockLevelParams();
    uint64_t makeWarehouseId();
    uint64_t makeDistrictId();
    uint64_t makeCustomerId();
    uint64_t makeItemId();
};
class Tpcc
{
    Loader *loader;
    DBDriver *driver;
    ScaleParameters *param;
    Random *rand;
    Executor *executor;

public:
    Tpcc(uint64_t warehouses,
         double scalefactor,
         std::map<std::string, const char *> config,
         IDBClient *client, bool load = true)
    {
        this->driver = new DBDriver(client);
        this->driver->loadConfig(config, load);
        this->param = ScaleParameters::makeWithScaleFactor(warehouses, scalefactor);
        std::vector<uint64_t> w_ids = {};
        for (uint64_t w_id = this->param->starting_warehouse; w_id < this->param->ending_warehouse + 1; w_id++)
        {
            w_ids.push_back(w_id);
        }
        this->rand = new Random();
        this->rand->setNURand(this->rand->makeForLoad());
        this->loader = new Loader(this->driver, this->param, w_ids, true, this->rand);
        this->executor = new Executor(this->driver, this->param, true, this->rand);
    }
    void run();
    std::chrono::duration<double> load();
    Results execute(std::chrono::duration<double> duration);
};
#endif
