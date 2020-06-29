#include "tpcc.h"

void DBDriver::loadConfig(std::map<std::string, const char *> config, bool load)
{
	this->client->connect(config["server.name"]);
	if (config.find("reset") != config.end())
	{
		this->client->execute(config["drop.tables"]);
	}
	if (load)
	{
		this->client->execute(config["init.tables"]);
	}
}

int DBDriver::executeTransaction(TransactionName txn, OpParam param)
{

	if (txn == DELIVERY)
	{
		return this->doDelivery(param);
	}
	else if (txn == NEW_ORDER)
	{
		return this->doNewOrder(param);
	}
	else if (txn == ORDER_STATUS)
	{
		return this->doOrderStatus(param);
	}
	else if (txn == PAYMENT)
	{
		return this->doPayment(param);
	}
	else if (txn == STOCK_LEVEL)
	{
		return this->doStockLevel(param);
	}
	else
	{
		DIGGI_ASSERT(false);
	}
	return -1;
}

void DBDriver::loadFinish(void)
{
	this->client->commit();
}

int DBDriver::doDelivery(OpParam param)
{
	this->client->beginTransaction();
	auto deliveryqueries = transaction_queries["DELIVERY"];
	for (uint64_t d_id = 1; d_id < TPCC_DISTRICTS_PER_WAREHOUSE + 1; d_id++)
	{
		this->client->execute(deliveryqueries["getNewOrder"], d_id, param.w_id);

		auto newOrder = this->client->fetchone();
		if (newOrder.columnCount() == 0)
		{
			continue;
		}

		auto no_o_id = newOrder.getUnsigned(0);

		this->client->execute(deliveryqueries["getCId"], no_o_id, d_id, param.w_id);
		auto c_id = this->client->fetchone().getUnsigned(0);

		this->client->execute(deliveryqueries["sumOLAmount"], no_o_id, d_id, param.w_id);
		auto ol_total = this->client->fetchone().getDouble(0);

		this->client->execute(deliveryqueries["deleteNewOrder"], d_id, param.w_id, no_o_id);
		this->client->execute(deliveryqueries["updateOrders"], param.o_carrier_id, no_o_id, d_id, param.w_id);
		this->client->execute(deliveryqueries["updateOrderLine"], param.ol_delivery_d.c_str(), no_o_id, d_id, param.w_id);

		this->client->execute(deliveryqueries["updateCustomer"], ol_total, c_id, d_id, param.w_id);
	}
	this->client->commit();
	return 0;
}

int DBDriver::doNewOrder(OpParam param)
{
	this->client->beginTransaction();
	auto neworderqueries = transaction_queries["NEW_ORDER"];
	DIGGI_ASSERT(param.i_ids.size() > 0);
	DIGGI_ASSERT(param.i_ids.size() == param.i_w_ids.size());
	DIGGI_ASSERT(param.i_ids.size() == param.i_qtys.size());
	bool all_local = true;
	std::vector<DBResult> items = {};
	for (uint64_t i = 0; i < param.i_ids.size(); i++)
	{
		all_local = all_local && (param.i_w_ids[i] == param.w_id);
		this->client->execute(neworderqueries["getItemInfo"], param.i_ids[i]);
		items.push_back(this->client->fetchone());
	}
	//TPCC defines that 1% of neworder gives wrong transaction id and should fail.
	DIGGI_ASSERT(items.size() == param.i_ids.size());
	for (auto item : items)
	{
		if (item.columnCount() == 0)
		{
			this->client->rollback();
			return -1;
		}
	}
	this->client->execute(neworderqueries["getWarehouseTaxRate"], param.w_id);

	this->client->execute(neworderqueries["getDistrict"], param.d_id, param.w_id);
	auto district_info = this->client->fetchone();
	auto d_next_o_id = district_info.getUnsigned(1);

	this->client->execute(neworderqueries["getCustomer"],
						  param.w_id,
						  param.d_id,
						  param.c_id);

	auto ol_cnt = param.i_ids.size();
	auto o_carrier_id = TPCC_NULL_CARRIER_ID;

	this->client->execute(neworderqueries["incrementNextOrderId"],
						  d_next_o_id + 1,
						  param.d_id,
						  param.w_id);
	this->client->execute(neworderqueries["createOrder"],
						  d_next_o_id,
						  param.d_id,
						  param.w_id,
						  param.c_id,
						  param.o_entry_d.c_str(),
						  o_carrier_id,
						  ol_cnt,
						  all_local);
	this->client->execute(neworderqueries["createNewOrder"],
						  d_next_o_id,
						  param.d_id,
						  param.w_id);

	for (uint64_t i = 0; i < param.i_ids.size(); i++)
	{
		auto ol_number = i + 1;
		auto ol_supply_w_id = param.i_w_ids[i];
		auto ol_i_id = param.i_ids[i];
		auto ol_quantity = param.i_qtys[i];
		auto iteminfo = items[i];
		auto i_name = iteminfo.getString(1);
		auto i_data = iteminfo.getString(2);
		auto i_price = iteminfo.getDouble(0);

		this->client->execute(neworderqueries["getStockInfo"],
							  param.d_id,
							  ol_i_id,
							  ol_supply_w_id);
		auto stockinfo = this->client->fetchone();
		if (stockinfo.columnCount() == 0)
		{
			/*
				Apparently not a problem
			*/
			continue;
		}
		auto s_quantity = stockinfo.getUnsigned(0);
		auto s_ytd = stockinfo.getUnsigned(2);
		auto s_order_cnt = stockinfo.getUnsigned(3);
		auto s_remote_cnt = stockinfo.getUnsigned(4);
		auto s_data = stockinfo.getString(1);
		auto s_dist_xx = stockinfo.getString(5);
		s_ytd += (double)ol_quantity;
		if (s_quantity >= (ol_quantity + 10))
		{
			s_quantity = s_quantity - ol_quantity;
		}
		else
		{
			s_quantity = s_quantity + (91 - (double)ol_quantity);
		}
		s_order_cnt += 1;
		if (ol_supply_w_id != param.w_id)
		{
			s_remote_cnt += 1;
		}
		this->client->execute(neworderqueries["updateStock"],
							  s_quantity,
							  s_ytd,
							  s_order_cnt,
							  s_remote_cnt,
							  ol_i_id,
							  ol_supply_w_id);
		auto ol_amount = ((double)ol_quantity) * i_price;
		DIGGI_ASSERT(ol_amount > 0.0);
		this->client->execute(neworderqueries["createOrderLine"],
							  d_next_o_id,
							  param.d_id,
							  param.w_id,
							  ol_number,
							  ol_i_id,
							  ol_supply_w_id,
							  param.o_entry_d.c_str(),
							  ol_quantity,
							  ol_amount,
							  s_dist_xx.c_str());
	}
	this->client->commit();
	return 0;
}

int DBDriver::doOrderStatus(OpParam param)
{
	auto orderstatusqueries = transaction_queries["ORDER_STATUS"];
	if (param.c_id != 0)
	{
		this->client->execute(orderstatusqueries["getCustomerByCustomerId"],
							  param.w_id,
							  param.d_id,
							  param.c_id);
	}
	else
	{
		this->client->execute(orderstatusqueries["getCustomersByLastName"],
							  param.w_id,
							  param.d_id,
							  param.c_last.c_str());
		auto all_customers = this->client->fetchall();
		DIGGI_ASSERT(all_customers.size() > 0);
		auto namecnt = all_customers.size();
		auto index = ((namecnt - 1) / 2);
		auto customer = all_customers[index];
		param.c_id = customer.getUnsigned(0);
	}

	DIGGI_ASSERT(param.c_id != 0);
	this->client->execute(orderstatusqueries["getLastOrder"],
						  param.w_id,
						  param.d_id,
						  param.c_id);
	auto order = this->client->fetchone();
	if (order.columnCount() > 0)
	{
		this->client->execute(orderstatusqueries["getOrderLines"],
							  param.w_id,
							  param.d_id,
							  order.getUnsigned(0));
	}
	this->client->commit();
	return 0;
}

int DBDriver::doPayment(OpParam param)
{
	this->client->beginTransaction();
	auto paymentqueries = transaction_queries["PAYMENT"];
	DBResult customer;
	if (param.c_id != 0)
	{
		this->client->execute(paymentqueries["getCustomerByCustomerId"],
							  param.w_id,
							  param.d_id,
							  param.c_id);
		customer = this->client->fetchone();
	}
	else
	{
		this->client->execute(paymentqueries["getCustomersByLastName"],
							  param.w_id,
							  param.d_id,
							  param.c_last.c_str());
		auto all_customers = this->client->fetchall();
		DIGGI_ASSERT(all_customers.size() > 0);
		auto namecnt = all_customers.size();
		auto index = ((namecnt - 1) / 2);
		customer = all_customers[index];
		param.c_id = customer.getUnsigned(0);
	}
	DIGGI_ASSERT(customer.columnCount() > 0);
	auto c_balance = customer.getDouble(14) - param.h_amount;
	auto c_ytd_payment = customer.getDouble(15) + param.h_amount;
	auto c_payment_cnt = customer.getUnsigned(16) + 1;
	auto c_data = customer.getString(17);

	this->client->execute(paymentqueries["getWarehouse"], param.w_id);
	auto warehouse = this->client->fetchone();
	this->client->execute(paymentqueries["getDistrict"], param.w_id, param.d_id);
	auto district = this->client->fetchone();

	this->client->execute(paymentqueries["updateWarehouseBalance"],
						  param.h_amount,
						  param.w_id);
	this->client->execute(paymentqueries["updateDistrictBalance"],
						  param.h_amount,
						  param.w_id,
						  param.d_id);

	if (customer.getString(11).compare(TPCC_BAD_CREDIT) == 0)
	{
		std::ostringstream os;
		os << param.c_id << " ";
		os << param.c_d_id << " ";
		os << param.c_w_id << " ";
		os << param.d_id << " ";
		os << param.w_id << " ";
		os << param.h_amount;
		auto newdata = os.str();
		c_data = newdata + "|" + c_data;
		if (c_data.size() > TPCC_MAX_C_DATA)
		{
			c_data = c_data.substr(0, TPCC_MAX_C_DATA);
		}
		this->client->execute(paymentqueries["updateBCCustomer"],
							  c_balance,
							  c_ytd_payment,
							  c_payment_cnt,
							  c_data.c_str(),
							  param.c_w_id,
							  param.c_d_id,
							  param.c_id);
	}
	else
	{
		c_data = "";
		this->client->execute(paymentqueries["updateGCCustomer"],
							  c_balance,
							  c_ytd_payment,
							  c_payment_cnt,
							  param.c_w_id,
							  param.c_d_id,
							  param.c_id);
	}
	auto h_data = warehouse.getString(0) + "    " + district.getString(0);
	this->client->execute(paymentqueries["insertHistory"],
						  param.c_id,
						  param.c_d_id,
						  param.c_w_id,
						  param.d_id,
						  param.w_id,
						  param.h_date.c_str(),
						  param.h_amount,
						  h_data.c_str());
	this->client->commit();
	return 0;
}

int DBDriver::doStockLevel(OpParam param)
{
	auto stockqueries = transaction_queries["STOCK_LEVEL"];

	this->client->execute(stockqueries["getOId"], param.w_id, param.d_id);
	auto result = this->client->fetchone();
	auto o_id = result.getUnsigned(0);
	this->client->execute(stockqueries["getStockCount"],
						  param.w_id,
						  param.d_id,
						  o_id,
						  (o_id - 20),
						  param.w_id,
						  param.threshold);
	this->client->commit();
	return 0;
}

void Loader::execute(void)
{
	if (this->needLoadItems)
	{
		loadItems();
	}
	for (auto w_id : this->w_ids)
	{
		loadWarehouse(w_id);
	}
}

void Loader::loadItems(void)
{
	auto originalRows = this->rand->selectUniqueIds(
		this->scaleParameters->items / 10,
		1,
		this->scaleParameters->items);

	std::vector<Item> tuples = std::vector<Item>();
	for (uint64_t i = 1; i < scaleParameters->items + 1; i++)
	{
		auto original = std::find(originalRows.begin(), originalRows.end(), i) != originalRows.end();
		tuples.push_back(generateItem(i, original));
		if (tuples.size() == this->batchSize)
		{
			this->handle->loadTuples(TPCC_TABLENAME_ITEM, tuples);
			tuples.clear();
		}
	}
	if (tuples.size() > 0)
	{
		this->handle->loadTuples(TPCC_TABLENAME_ITEM, tuples);
	}
}

void Loader::loadWarehouse(uint64_t w_id)
{
	std::vector<Warehouse> w_tuples = {generateWarehouse(w_id)};
	this->handle->loadTuples(TPCC_TABLENAME_WAREHOUSE, w_tuples);
	std::vector<District> d_tuples = {};
	for (uint64_t d_id = 1; d_id < this->scaleParameters->districts_per_warehouse + 1; d_id++)
	{
		auto d_next_o_id = this->scaleParameters->customers_per_district + 1;
		std::vector<District> d_tuples = {generateDistrict(w_id, d_id, d_next_o_id)};
		std::vector<Customer> c_tuples = {};
		std::vector<History> h_tuples = {};
		auto selectedRows = this->rand->selectUniqueIds(
			this->scaleParameters->customers_per_district / 10, 1,
			this->scaleParameters->customers_per_district);
		std::vector<uint64_t> cid_permutation = {};
		for (uint64_t c_id = 1; c_id < this->scaleParameters->customers_per_district + 1; c_id++)
		{
			auto badcredit = std::find(selectedRows.begin(), selectedRows.end(), c_id) != selectedRows.end();
			c_tuples.push_back(generateCustomer(w_id, d_id, c_id, badcredit));
			h_tuples.push_back(generateHistory(w_id, d_id, c_id));
			cid_permutation.push_back(c_id);
		}
		DIGGI_ASSERT(cid_permutation[0] == 1);
		DIGGI_ASSERT(
			cid_permutation[this->scaleParameters->customers_per_district - 1] == this->scaleParameters->customers_per_district);
		std::random_shuffle(cid_permutation.begin(), cid_permutation.end());

		std::vector<Order> o_tuples = {};
		std::vector<OrderLine> ol_tuples = {};
		std::vector<NewOrder> no_tuples = {};
		for (uint64_t o_id = 1; o_id < this->scaleParameters->customers_per_district + 1; o_id++)
		{
			auto o_ol_cnt = this->rand->number(TPCC_MIN_OL_CNT, TPCC_MAX_OL_CNT);
			auto newOrder = ((this->scaleParameters->customers_per_district - this->scaleParameters->new_orders_per_district) < o_id);

			o_tuples.push_back(
				generateOrder(
					w_id,
					d_id,
					o_id,
					cid_permutation[o_id - 1],
					o_ol_cnt,
					newOrder));
			for (uint64_t ol_number = 0; ol_number < o_ol_cnt; ol_number++)
			{
				ol_tuples.push_back(generateOrderLine(
					w_id,
					d_id,
					o_id,
					ol_number,
					this->scaleParameters->items,
					newOrder));
			}
			if (newOrder)
			{
				no_tuples.push_back(NewOrder(o_id, d_id, w_id));
			}
		}

		this->handle->loadTuples(TPCC_TABLENAME_DISTRICT, d_tuples);
		this->handle->loadTuples(TPCC_TABLENAME_CUSTOMER, c_tuples);
		this->handle->loadTuples(TPCC_TABLENAME_ORDERS, o_tuples);
		this->handle->loadTuples(TPCC_TABLENAME_ORDER_LINE, ol_tuples);
		this->handle->loadTuples(TPCC_TABLENAME_NEW_ORDER, no_tuples);
		this->handle->loadTuples(TPCC_TABLENAME_HISTORY, h_tuples);
	}
	std::vector<Stock> s_tuples = {};
	auto selectedRows = this->rand->selectUniqueIds(
		this->scaleParameters->items / 10,
		1,
		this->scaleParameters->items);
	for (uint64_t i_id = 1; i_id < this->scaleParameters->items + 1; i_id++)
	{
		auto original = std::find(selectedRows.begin(), selectedRows.end(), i_id) != selectedRows.end();
		s_tuples.push_back(generateStock(w_id, i_id, original));
		if (s_tuples.size() >= this->batchSize)
		{
			this->handle->loadTuples(TPCC_TABLENAME_STOCK, s_tuples);
			s_tuples.clear();
		}
	}
	if (s_tuples.size() > 0)
	{
		this->handle->loadTuples(TPCC_TABLENAME_STOCK, s_tuples);
	}
}

Item Loader::generateItem(uint64_t id, bool original)
{
	auto i_id = id;
	auto i_im_id = this->rand->number(TPCC_MIN_IM, TPCC_MAX_IM);
	auto i_name = this->rand->astring(TPCC_MIN_I_NAME, TPCC_MAX_I_NAME);
	auto i_price = this->rand->fixedPoint(TPCC_MONEY_DECIMALS, TPCC_MIN_PRICE, TPCC_MAX_PRICE);
	auto i_data = this->rand->astring(TPCC_MIN_I_DATA, TPCC_MAX_I_DATA);
	if (original)
	{
		i_data = fillOriginal(i_data);
	}
	return Item(i_id, i_im_id, i_name, i_price, i_data);
}

Warehouse Loader::generateWarehouse(uint64_t w_id)
{
	auto w_tax = generateTax();
	auto w_ytd = TPCC_INITIAL_W_YTD;
	auto w_address = generateAddress();
	return Warehouse(w_id, w_address, w_tax, w_ytd);
}

District Loader::generateDistrict(uint64_t d_w_id, uint64_t d_id, uint64_t d_next_o_id)
{
	auto d_tax = generateTax();
	auto d_ytd = TPCC_INITIAL_D_YTD;
	auto d_address = generateAddress();
	return District(d_id, d_w_id, d_address, d_tax, d_ytd, d_next_o_id);
}

Customer Loader::generateCustomer(uint64_t c_w_id, uint64_t c_d_id, uint64_t c_id, bool badCredit)
{
	auto c_first = this->rand->astring(TPCC_MIN_FIRST, TPCC_MAX_FIRST);
	std::string c_middle = TPCC_MIDDLE;
	DIGGI_ASSERT((1 <= c_id) && (c_id <= TPCC_CUSTOMERS_PER_DISTRICT));
	std::string c_last = "";
	if (c_id <= 1000)
	{
		c_last = this->rand->makeLastName(c_id - 1);
	}
	else
	{
		c_last = this->rand->makeRandomLastName(TPCC_CUSTOMERS_PER_DISTRICT);
	}

	auto c_phone = this->rand->nstring(TPCC_PHONE, TPCC_PHONE);
	auto c_since = this->rand->datetimeNow();
	//(condition) ? (if_true) : (if_false)
	std::string c_credit = (badCredit) ? TPCC_BAD_CREDIT : TPCC_GOOD_CREDIT;
	auto c_credit_lim = TPCC_INITIAL_CREDIT_LIM;
	auto c_discount = this->rand->fixedPoint(TPCC_DISCOUNT_DECIMALS, TPCC_MIN_DISCOUNT, TPCC_MAX_DISCOUNT);
	auto c_balance = TPCC_INITIAL_BALANCE;
	auto c_ytd_payment = TPCC_INITIAL_YTD_PAYMENT;
	auto c_payment_cnt = TPCC_INITIAL_PAYMENT_CNT;
	auto c_delivery_cnt = TPCC_INITIAL_DELIVERY_CNT;
	auto c_data = this->rand->astring(TPCC_MIN_C_DATA, TPCC_MAX_C_DATA);
	auto c_street1 = this->rand->astring(TPCC_MIN_STREET, TPCC_MAX_STREET);
	auto c_street2 = this->rand->astring(TPCC_MIN_STREET, TPCC_MAX_STREET);
	auto c_city = this->rand->astring(TPCC_MIN_CITY, TPCC_MAX_CITY);
	auto c_state = this->rand->astring(TPCC_STATE, TPCC_STATE);
	auto c_zip = generateZip();
	return Customer(
		c_id,
		c_d_id,
		c_w_id,
		c_first,
		c_middle,
		c_last,
		c_street1,
		c_street2,
		c_city,
		c_state,
		c_zip,
		c_phone,
		c_since,
		c_credit,
		c_credit_lim,
		c_discount,
		c_balance,
		c_ytd_payment,
		c_payment_cnt,
		c_delivery_cnt,
		c_data);
}

Order Loader::generateOrder(
	uint64_t o_w_id,
	uint64_t o_d_id,
	uint64_t o_id,
	uint64_t o_c_id,
	uint64_t o_ol_cnt,
	bool newOrder)
{
	auto o_entry_d = this->rand->datetimeNow();
	auto o_carrier_id = (newOrder)
							? TPCC_NULL_CARRIER_ID
							: this->rand->number(TPCC_MIN_CARRIER_ID, TPCC_MAX_CARRIER_ID);
	auto o_all_local = TPCC_INITIAL_ALL_LOCAL;
	return Order(o_id,
				 o_c_id,
				 o_d_id,
				 o_w_id,
				 o_entry_d,
				 o_carrier_id,
				 o_ol_cnt,
				 o_all_local);
}

OrderLine Loader::generateOrderLine(uint64_t ol_w_id, uint64_t ol_d_id, uint64_t ol_o_id, uint64_t ol_number, uint64_t max_items, bool newOrder)
{
	auto ol_i_id = this->rand->number(1, max_items);
	auto ol_supply_w_id = ol_w_id;
	auto ol_delivery_d = this->rand->datetimeNow();
	auto ol_quantity = TPCC_INITIAL_QUANTITY;
	double ol_amount = 0.00;
	if (newOrder)
	{
		ol_amount = 0.00;
	}
	else
	{
		ol_amount = this->rand->fixedPoint(
			TPCC_MONEY_DECIMALS,
			TPCC_MIN_AMOUNT,
			TPCC_MAX_PRICE * TPCC_MAX_OL_QUANTITY);
		ol_delivery_d = "";
	}
	auto ol_dist_info = this->rand->astring(TPCC_DIST, TPCC_DIST);
	return OrderLine(
		ol_o_id,
		ol_d_id,
		ol_w_id,
		ol_number,
		ol_i_id,
		ol_supply_w_id,
		ol_delivery_d,
		ol_quantity,
		ol_amount,
		ol_dist_info);
}

Stock Loader::generateStock(uint64_t s_w_id, uint64_t s_i_id, bool original)
{
	auto s_quantity = this->rand->number(TPCC_MIN_QUANTITY, TPCC_MAX_QUANTITY);
	auto s_ytd = 0;
	auto s_order_cnt = 0;
	auto s_remote_cnt = 0;
	auto s_data = this->rand->astring(TPCC_MIN_I_DATA, TPCC_MAX_I_DATA);
	if (original)
	{
		fillOriginal(s_data);
	}
	std::vector<std::string> s_dists = {};
	for (int i = 0; i < TPCC_DISTRICTS_PER_WAREHOUSE; i++)
	{
		s_dists.push_back(this->rand->astring(TPCC_DIST, TPCC_DIST));
	}
	return Stock(
		s_i_id,
		s_w_id,
		s_quantity,
		s_dists,
		s_ytd,
		s_order_cnt,
		s_remote_cnt,
		s_data);
}

History Loader::generateHistory(uint64_t h_c_w_id, uint64_t h_c_d_id, uint64_t h_c_id)
{
	auto h_date = this->rand->datetimeNow();

	auto h_data = this->rand->astring(TPCC_MIN_DATA, TPCC_MAX_DATA);
	return History(
		h_c_id,
		h_c_d_id,
		h_c_w_id,
		h_c_d_id,
		h_c_w_id,
		h_date,
		TPCC_INITIAL_AMOUNT,
		h_data);
}

Address Loader::generateAddress(void)
{
	auto name = this->rand->astring(TPCC_MIN_NAME, TPCC_MAX_NAME);
	auto addr = generateStreetAddress();
	addr.name = name;
	return addr;
}

Address Loader::generateStreetAddress(void)
{
	auto street1 = this->rand->astring(TPCC_MIN_STREET, TPCC_MAX_STREET);
	auto street2 = this->rand->astring(TPCC_MIN_STREET, TPCC_MAX_STREET);
	auto city = this->rand->astring(TPCC_MIN_CITY, TPCC_MAX_CITY);
	auto state = this->rand->astring(TPCC_STATE, TPCC_STATE);
	auto zip = generateZip();
	return Address(
		"",
		street1,
		street2,
		city,
		state,
		zip);
}

double Loader::generateTax(void)
{
	return this->rand->fixedPoint(TPCC_TAX_DECIMALS, TPCC_MIN_TAX, TPCC_MAX_TAX);
}

std::string Loader::generateZip(void)
{
	auto length = TPCC_ZIP_LENGTH - sizeof(TPCC_ZIP_SUFFIX);
	return this->rand->nstring(length, length) + TPCC_ZIP_SUFFIX;
}

std::string Loader::fillOriginal(std::string data)
{
	auto originalLength = sizeof(TPCC_ORIGINAL_STRING);
	auto position = this->rand->number(0, data.size() - originalLength);
	auto out = data.substr(0, position) + TPCC_ORIGINAL_STRING + data.substr(position + originalLength - 1, data.size() - (position + originalLength - 1));
	DIGGI_ASSERT(data.size() == out.size());
	return out;
}

NURandC *Random::makeForLoad()
{
	auto ret = new NURandC();
	ret->cLast = number(0, 255);
	ret->cId = number(0, 1023);
	ret->orderrLineItemId = number(0, 8191);
	return ret;
}

void Random::setNURand(NURandC *val)
{
	this->nurandVar = val;
}

uint64_t Random::NURand(uint64_t a, uint64_t x, uint64_t y)
{
	DIGGI_ASSERT(x < y);
	uint64_t c = 0;
	if (!this->nurandVar)
	{
		setNURand(makeForLoad());
	}
	if (a == 255)
	{
		c = this->nurandVar->cLast;
	}
	else if (a == 1023)
	{
		c = this->nurandVar->cId;
	}
	else if (a == 8191)
	{
		c = nurandVar->orderrLineItemId;
	}
	else
	{
		DIGGI_ASSERT(false);
	}
	return (((number(0, a) | number(x, y)) + c) % (y - x + 1)) + x;
}

uint64_t Random::number(uint64_t min, uint64_t max)
{
	DIGGI_ASSERT(max >= min);
	if (min == max)
	{
		return min;
	}
	auto val = (rand() % (max - min) + min);
	DIGGI_ASSERT((val >= min) && (val <= max));
	return val;
}

uint64_t Random::numberExcluding(uint64_t min, uint64_t max, uint64_t excluding)
{
	DIGGI_ASSERT(max > min);
	DIGGI_ASSERT((min <= excluding) && (excluding <= max));
	auto num = number(min, max - 1);
	if (num >= excluding)
	{
		num += 1;
	}
	DIGGI_ASSERT((min <= num) && (num <= max) && (num != excluding));
	return num;
}

double Random::fixedPoint(uint64_t decimalplaces, double min, double max)
{
	DIGGI_ASSERT(decimalplaces > 0);
	DIGGI_ASSERT(min < max);
	auto multiplier = 1;
	for (uint64_t i = 0; i < decimalplaces; i++)
	{
		multiplier *= 10;
	}
	int64_t int_min = min * multiplier + 0.5;
	int64_t int_max = max * multiplier + 0.5;

	return double(number(int_min, int_max) / double(multiplier));
}

std::string Random::datetimeNow()
{
	auto now = std::chrono::system_clock::now();
	std::time_t tt = std::chrono::system_clock::to_time_t(now);
	auto str = std::string(ctime(&tt));
	str.erase(str.length() - 1);
	return str;
}

std::vector<uint64_t> Random::selectUniqueIds(uint64_t numUnique, uint64_t min, uint64_t max)
{
	auto rows = std::vector<uint64_t>();
	for (uint64_t i = 0; i < numUnique; i++)
	{
		auto index = UINT64_MAX;
		while ((index == UINT64_MAX) || (std::find(rows.begin(), rows.end(), index) != rows.end()))
		{
			index = number(min, max);
		}
		rows.push_back(index);
	}
	DIGGI_ASSERT(rows.size() == numUnique);
	return rows;
}
std::string Random::astring(uint64_t minlen, uint64_t maxlen)
{
	return randomString(minlen, maxlen, 'a', 26);
}

std::string Random::nstring(uint64_t minlen, uint64_t maxlen)
{
	return randomString(minlen, maxlen, '0', 10);
}

std::string Random::randomString(uint64_t min, uint64_t max, char base, uint64_t numCharacters)
{
	auto length = number(min, max);
	auto basebyte = (uint64_t)base;
	std::string retstr = "";
	for (uint64_t i = 0; i < length; i++)
	{
		retstr += ((char)(basebyte + number(0, numCharacters - 1)));
	}
	return retstr;
}

std::string Random::makeLastName(uint64_t number)
{
	DIGGI_ASSERT((0 <= number) && (number <= 999));
	auto retstring = std::string();
	retstring += this->syllables[number / 100];
	retstring += this->syllables[(number / 10) % 10];
	retstring += this->syllables[number % 10];
	return retstring;
}

std::string Random::makeRandomLastName(uint64_t max_c_id)
{
	auto min_cid = 999u;
	DIGGI_ASSERT(max_c_id > 1);
	if ((max_c_id - 1u) < min_cid)
	{
		min_cid = max_c_id - 1u;
	}
	return makeLastName(NURand(255, 0, min_cid));
}

ScaleParameters *ScaleParameters::makeWithScaleFactor(uint64_t warehouses, double scaleFactor)
{
	DIGGI_ASSERT(scaleFactor >= 1.0);
	auto items = (uint64_t)(TPCC_NUM_ITEMS / scaleFactor);
	auto districts = (uint64_t)std::max(TPCC_DISTRICTS_PER_WAREHOUSE, 1);
	auto customers = (uint64_t)std::max((uint64_t)(TPCC_CUSTOMERS_PER_DISTRICT / scaleFactor), (uint64_t)1);
	auto newOrders = (uint64_t)std::max((uint64_t)(TPCC_INITIAL_NEW_ORDERS_PER_DISTRICT / scaleFactor), (uint64_t)0);
	return new ScaleParameters(items, warehouses, districts, customers, newOrders);
}

ScaleParameters *ScaleParameters::makeDefault(uint64_t warehouses)
{
	return new ScaleParameters(
		TPCC_NUM_ITEMS,
		warehouses,
		TPCC_DISTRICTS_PER_WAREHOUSE,
		TPCC_CUSTOMERS_PER_DISTRICT,
		TPCC_INITIAL_NEW_ORDERS_PER_DISTRICT);
}

void Tpcc::run()
{
	auto loadtime = this->load();
	/*
		more config for time
	*/
	this->execute(std::chrono::duration<double>(1000));
}

std::chrono::duration<double> Tpcc::load()
{
	auto pre = std::chrono::system_clock::now();
	/*fix for multiple clients*/
	this->loader->execute();
	auto post = std::chrono::system_clock::now();
	return post - pre;
}

Results Tpcc::execute(std::chrono::duration<double> duration)
{
	return this->executor->execute(duration);
}

std::chrono::system_clock::time_point Results::startBenchmark()
{
	this->start = std::chrono::system_clock::now();
	return this->start;
}

void Results::stopBenchmark()
{
	this->stop = std::chrono::system_clock::now();
}

uint64_t Results::startTransaction(TransactionName tp)
{
	this->txn_id += 1;
	auto id = this->txn_id;
	this->running[id] = std::make_tuple(tp, std::chrono::system_clock::now());
	return id;
}

void Results::abortTransaction(uint64_t id)
{
	this->running.erase(id);
}

void Results::stopTransaction(uint64_t id)
{
	auto txtup = this->running[id];
	this->running.erase(id);
	auto duration = std::chrono::system_clock::now() - std::get<1>(txtup);
	auto total_time = std::chrono::duration<double>(0);
	if (this->txn_times.count(std::get<0>(txtup)))
	{
		total_time = this->txn_times[std::get<0>(txtup)];
	}
	this->txn_times[std::get<0>(txtup)] = total_time + duration;
	uint64_t total_cnt = 0;
	if (this->txn_counters.count(std::get<0>(txtup)))
	{
		total_cnt = this->txn_counters[std::get<0>(txtup)];
	}
	this->txn_counters[std::get<0>(txtup)] = total_cnt + 1;
}

void Results::append(Results r)
{

	for (std::map<TransactionName, uint64_t>::iterator iter = r.txn_counters.begin(); iter != r.txn_counters.end(); ++iter)
	{
		uint64_t orig_cnt = 0;
		if (this->txn_counters.count(iter->first))
		{
			orig_cnt = this->txn_counters[iter->first];
		}
		auto orig_time = std::chrono::duration<double>(0);
		if (this->txn_times.count(iter->first))
		{
			orig_time = this->txn_times[iter->first];
		}
		this->txn_counters[iter->first] = orig_cnt + r.txn_counters[iter->first];
		this->txn_times[iter->first] = orig_time + r.txn_times[iter->first];
	}
	this->start = r.start;
	this->stop = r.stop;
}

void Results::tostring()
{
	//TODO
}

Results Executor::execute(std::chrono::duration<double> duration)
{
	auto results = Results();
	auto start = results.startBenchmark();
	while ((std::chrono::system_clock::now() - start) < duration)
	{
		auto ret = this->doOne();
		auto txn_id = results.startTransaction(std::get<0>(ret));
		if (this->driver->executeTransaction(std::get<0>(ret), std::get<1>(ret)) < 0)
		{
			results.abortTransaction(txn_id);
		}
		else
		{
			results.stopTransaction(txn_id);
		}
	}
	results.stopBenchmark();
	return results;
}

OpParam Executor::generateNewOrderParams()
{
	auto p = OpParam();
	p.w_id = this->makeWarehouseId();
	p.d_id = this->makeDistrictId();
	p.c_id = this->makeCustomerId();
	auto ol_cnt = this->rand->number(TPCC_MIN_OL_CNT, TPCC_MAX_OL_CNT);
	p.o_entry_d = this->rand->datetimeNow();
	auto rollback = (this->rand->number(1, 100) == 1);

	std::vector<uint64_t> i_ids = {};
	std::vector<uint64_t> i_w_ids = {};
	std::vector<uint64_t> i_qtys = {};
	for (uint64_t i = 0; i < ol_cnt; i++)
	{
		if (rollback && i + 1 == ol_cnt)
		{
			i_ids.push_back(this->param->items + 1);
		}
		else
		{
			auto i_id = this->makeItemId();
			while (std::find(i_ids.begin(), i_ids.end(), i_id) != i_ids.end())
			{
				i_id = this->makeItemId();
			}
			i_ids.push_back(i_id);
		}
		auto remote = (rand->number(1, 100) == 1);
		if ((this->param->warehouses > 1) && remote)
		{
			i_w_ids.push_back(this->rand->numberExcluding(this->param->starting_warehouse, this->param->ending_warehouse, p.w_id));
		}
		else
		{
			i_w_ids.push_back(p.w_id);
		}
		i_qtys.push_back(this->rand->number(1, TPCC_MAX_OL_QUANTITY));
	}
	p.i_ids = i_ids;
	p.i_w_ids = i_w_ids;
	p.i_qtys = i_qtys;
	return p;
}

OpParam Executor::generatePaymentParams()
{
	auto p = OpParam();
	auto x = this->rand->number(1, 100);
	auto y = this->rand->number(1, 100);
	p.w_id = this->makeWarehouseId();
	p.d_id = this->makeDistrictId();
	p.c_w_id = 0;
	p.c_d_id = 0;
	p.c_id = 0;
	p.c_last = "";
	p.h_amount = this->rand->fixedPoint(2, TPCC_MIN_PAYMENT, TPCC_MAX_PAYMENT);
	p.h_date = this->rand->datetimeNow();
	if ((this->param->warehouses == 1) || x <= 85)
	{
		p.c_w_id = p.w_id;
		p.c_d_id = p.d_id;
	}
	else
	{
		p.c_w_id = this->rand->numberExcluding(this->param->starting_warehouse, this->param->ending_warehouse, p.w_id);
		p.c_d_id = this->makeDistrictId();
	}
	if (y <= 60)
	{
		p.c_last = this->rand->makeRandomLastName(this->param->customers_per_district);
	}
	else
	{
		p.c_id = this->makeCustomerId();
	}
	return p;
}

OpParam Executor::generateOrderStatusParams()
{
	auto p = OpParam();
	p.w_id = this->makeWarehouseId();
	p.d_id = this->makeDistrictId();
	p.c_last = "";
	p.c_id = 0;
	if (this->rand->number(1, 100) <= 60)
	{

		p.c_last = this->rand->makeRandomLastName(this->param->customers_per_district);
	}
	else
	{
		p.c_id = this->makeCustomerId();
	}
	return p;
}

OpParam Executor::generateDeliveryParams()
{
	auto p = OpParam();
	p.w_id = this->makeWarehouseId();
	p.o_carrier_id = this->rand->number(TPCC_MIN_CARRIER_ID, TPCC_MAX_CARRIER_ID);
	p.ol_delivery_d = this->rand->datetimeNow();
	return p;
}

OpParam Executor::generateStockLevelParams()
{
	auto p = OpParam();
	p.w_id = this->makeWarehouseId();
	p.d_id = this->makeDistrictId();
	p.threshold = this->rand->number(TPCC_MIN_STOCK_LEVEL_THRESHOLD, TPCC_MAX_STOCK_LEVEL_THRESHOLD);
	return p;
}

uint64_t Executor::makeWarehouseId()
{
	return this->rand->number(this->param->starting_warehouse, this->param->ending_warehouse);
}

uint64_t Executor::makeDistrictId()
{
	return this->rand->number(1, this->param->districts_per_warehouse);
}

uint64_t Executor::makeCustomerId()
{
	return this->rand->NURand(1023, 1, this->param->customers_per_district);
}

uint64_t Executor::makeItemId()
{
	return this->rand->NURand(8191, 1, this->param->items);
}

std::tuple<TransactionName, OpParam> Executor::doOne()
{
	auto x = this->rand->number(1, 100);
	if (x <= 4)
	{
		return std::tuple<TransactionName, OpParam>(STOCK_LEVEL, this->generateStockLevelParams());
	}
	else if (x <= 4 + 4)
	{
		return std::tuple<TransactionName, OpParam>(DELIVERY, this->generateDeliveryParams());
	}
	else if (x <= 4 + 4 + 4)
	{
		return std::tuple<TransactionName, OpParam>(ORDER_STATUS, this->generateOrderStatusParams());
	}
	else if (x <= 43 + 4 + 4 + 4)
	{
		return std::tuple<TransactionName, OpParam>(PAYMENT, this->generatePaymentParams());
	}
	else
	{
		return std::tuple<TransactionName, OpParam>(NEW_ORDER, this->generateNewOrderParams());
	}
}