#include <gtest/gtest.h>
#include "NeuralNetwork.h"
#include <iterator>
#include <vector>
#include <stdio.h>
static const double data[10][3] = {
{2.7810836,		2.550537003,	0},
{1.465489372,	2.362125076,	0 },
{3.396561688,	4.400293529,	0 },
{1.38807019,	1.850220317,	0 },
{3.06407232,	3.005305973,	0 },
{7.627531214,	2.759262235,	1 },
{5.332441248,	2.088626775,	1 },
{6.922596716,	1.77106367,		1 },
{8.675418651,	-0.242068655,	1 },
{7.673756466,	3.508563011	,	1 }
};


TEST(neuralnetworktest, simple_learn) {
	auto nn = new NeuralNetwork(2, 2, 2);
	auto X =  new matrix_t();
	
	//Convert from static array to matrix
	for (unsigned i = 0; i < 10; i++) {
		auto row = new vector_t(std::begin(data[i]), std::end(data[i]));
		X->push_back(row);
	}

	delete nn->train(X, 0.5, 20, 2);
	
	for (unsigned i = 0; i < 10; i++) {
		auto sample = X->at(i);
		EXPECT_TRUE(nn->predict(sample) == sample->back());
	}

	for (unsigned i = 0; i < 10; i++) {
		auto ptr = X->back();
		delete ptr;
		X->pop_back();
	}

	delete X;
	delete nn;
}

TEST(neuralnetworktest, simple_store_retrieve) 
{

	auto nn = new NeuralNetwork(2, 2, 2);
	auto X = new matrix_t();

	//Convert from static array to matrix
	for (unsigned i = 0; i < 10; i++) {
		auto row = new vector_t(std::begin(data[i]), std::end(data[i]));
		X->push_back(row);
	}

	delete nn->train(X, 0.5, 20, 2);

	for (unsigned i = 0; i < 10; i++) {
		auto sample = X->at(i);
		EXPECT_TRUE(nn->predict(sample) == sample->back());
	}

	auto doc  = nn->exportToJson();

	auto nn2 = new NeuralNetwork(doc);
	for (unsigned i = 0; i < 10; i++) {
		auto sample = X->at(i);
		EXPECT_TRUE(nn2->predict(sample) == sample->back());
	}
	delete nn;
	delete nn2;
	delete doc;
	
	for (unsigned i = 0; i < 10; i++) {
		auto ptr = X->back();
		delete ptr;
		X->pop_back();
	}
	delete X;
}
