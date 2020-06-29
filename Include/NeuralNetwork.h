#ifndef NEURALNETWORK_H
#define NEURALNETWORK_H
#include <vector>
#include "DiggiAssert.h"
#include "JSONParser.h"
#include <math.h>
#include <algorithm>
#include <iterator>
typedef std::vector<double> vector_t;
typedef std::vector<vector_t*> matrix_t;
class Neuron {
public:
	vector_t *weights;
	double delta;
	double output;
};

typedef std::vector<Neuron*> layer_t;

class NeuralNetwork {
	
	layer_t *hidden_layer, *output_layer;
	std::vector<layer_t*> *layers;

	vector_t * forwardPropagate(vector_t* x);
	void backwardsPropagate(vector_t* y);
	void updateWeights(vector_t *x, double learn_rate);
	double sumOfErrorsSqrd(vector_t *y_hat, vector_t* y);
	double weightedSum(vector_t *w, vector_t * x);
	double sigmoid(double input);
	double sigmoidMarked(double input);
	double sumLayerError(size_t index_i, size_t index_j);
	double sqr(double val);
public:

	NeuralNetwork(size_t inputs, size_t hidden, size_t outputs);
	NeuralNetwork(json_document *document);
	~NeuralNetwork();
	json_document *exportToJson();
	vector_t* train(matrix_t* X, double learningRate, size_t epochs, size_t output_n);
	size_t predict(vector_t * x);

};





#endif