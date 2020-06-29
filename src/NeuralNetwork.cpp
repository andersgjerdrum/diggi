/**
 * @file neuralnetwork.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief Implementation of a multi-layer perceptron neural network.
 * @version 0.1
 * @date 2020-02-05
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "NeuralNetwork.h"
#include "DiggiGlobal.h"
/**
 * @brief Construct a new Neural Network:: Neural Network object
 * Create a neural network with a given set of inputs, hidden layers and output layers.
 * @param inputs 
 * @param hidden 
 * @param outputs 
 */
NeuralNetwork::NeuralNetwork(
    size_t inputs,
    size_t hidden,
    size_t outputs)
{
    layers = new std::vector<layer_t *>();

    hidden_layer = new layer_t();
    output_layer = new layer_t();
    layers->push_back(hidden_layer);
    layers->push_back(output_layer);
    for (unsigned i = 0; i < hidden; i++)
    {
        auto n = new Neuron();
        n->delta = 0.0;
        n->output = 0.0;

        int randomint = rand();
        double randofloat = static_cast<double>(randomint) / static_cast<double>(RAND_MAX);

        n->weights = new vector_t(inputs, randofloat);
        hidden_layer->push_back(n);
    }
    for (unsigned i = 0; i < outputs; i++)
    {
        auto n = new Neuron();
        n->delta = 0.0;
        n->output = 0.0;
        n->weights = new vector_t(hidden, 0.0);
        output_layer->push_back(n);
    }
}
/**
 * @brief Construct a new Neural Network:: Neural Network object
 * Construct neural network based on serialized json representation.
 * @param document 
 */
NeuralNetwork::NeuralNetwork(json_document *document)
{
    auto hidden_layer_packed = document->head["hidden"].children;
    auto output_layer_packed = document->head["output"].children;

    hidden_layer = new layer_t();
    output_layer = new layer_t();
    layers = new std::vector<layer_t *>();

    for (unsigned i = 0; i < hidden_layer_packed.size(); i++)
    {

        auto nrn_pack = hidden_layer_packed[i];
        auto weights_pack = nrn_pack["weights"].children;

        auto weights = new vector_t();
        auto nrn = new Neuron();
        for (unsigned k = 0; k < weights_pack.size(); k++)
        {
            auto val = (double *)(weights_pack[k].value.getmbuf()->head->data);
            weights->push_back(*val);
        }

        nrn->weights = weights;
        auto o_val = (double *)(nrn_pack["output"].value.getmbuf()->head->data);
        nrn->output = *o_val;
        auto d_val = (double *)(nrn_pack["delta"].value.getmbuf()->head->data);
        nrn->delta = *d_val;
        hidden_layer->push_back(nrn);
    }

    for (unsigned i = 0; i < output_layer_packed.size(); i++)
    {

        auto nrn_pack = output_layer_packed[i];
        auto weights_pack = nrn_pack["weights"].children;

        auto weights = new vector_t();
        auto nrn = new Neuron();
        for (unsigned k = 0; k < weights_pack.size(); k++)
        {
            auto val = (double *)(weights_pack[k].value.getmbuf()->head->data);
            weights->push_back(*val);
        }

        nrn->weights = weights;
        auto o_val = (double *)(nrn_pack["output"].value.getmbuf()->head->data);
        nrn->output = *o_val;
        auto d_val = (double *)(nrn_pack["delta"].value.getmbuf()->head->data);
        nrn->delta = *d_val;

        output_layer->push_back(nrn);
    }

    layers->push_back(hidden_layer);
    layers->push_back(output_layer);
}
/**
 * @brief Destroy the Neural Network:: Neural Network object
 * 
 */
NeuralNetwork::~NeuralNetwork()
{

    auto hidden = hidden_layer->size();
    for (unsigned i = 0; i < hidden; i++)
    {
        auto nrn = hidden_layer->back();
        DIGGI_ASSERT(nrn);
        DIGGI_ASSERT(nrn->weights);

        hidden_layer->pop_back();
        delete nrn->weights;
        delete nrn;
    }

    auto output = output_layer->size();
    for (unsigned i = 0; i < output; i++)
    {
        auto nrn = output_layer->back();
        DIGGI_ASSERT(nrn);
        DIGGI_ASSERT(nrn->weights);
        output_layer->pop_back();
        delete nrn->weights;
        delete nrn;
    }

    delete layers;
    delete hidden_layer;
    delete output_layer;
}
/**
 * @brief serialize trained network into JSON
 * 
 * @return json_document* 
 */
json_document *NeuralNetwork::exportToJson()
{

    json_node hidden;
    hidden.type = JSON;
    hidden.key = "hidden";
    json_node output;
    output.type = JSON;
    output.key = "output";

    for (unsigned i = 0; i < hidden_layer->size(); i++)
    {
        auto neuron = hidden_layer->at(i);
        json_node neuron_pack;
        neuron_pack.type = JSON;
        double *d_val = &(neuron->delta);
        double *o_val = &(neuron->output);

        neuron_pack << json_node("delta", zcstring((char *)d_val, 8));
        neuron_pack << json_node("output", zcstring((char *)o_val, 8));
        json_node neuron_weights_pack;
        neuron_weights_pack.key = "weights";
        neuron_weights_pack.type = ARRAY;

        for (unsigned k = 0; k < neuron->weights->size(); k++)
        {
            double *val = &(neuron->weights->at(k));
            neuron_weights_pack << json_node("", zcstring((char *)val, 8));
        }
        neuron_pack << neuron_weights_pack;
        hidden << neuron_pack;
    }

    for (unsigned i = 0; i < output_layer->size(); i++)
    {

        auto neuron = output_layer->at(i);
        json_node neuron_pack;
        neuron_pack.type = JSON;
        double *d_val = &(neuron->delta);
        double *o_val = &(neuron->output);

        neuron_pack << json_node("delta", zcstring((char *)d_val, 8));
        neuron_pack << json_node("output", zcstring((char *)o_val, 8));
        json_node neuron_weights_pack;
        neuron_weights_pack.key = "weights";
        neuron_weights_pack.type = ARRAY;

        for (unsigned k = 0; k < neuron->weights->size(); k++)
        {
            double *val = &(neuron->weights->at(k));
            neuron_weights_pack << json_node("", zcstring((char *)val, 8));
        }

        neuron_pack << neuron_weights_pack;
        output << neuron_pack;
    }

    json_node head;
    head << hidden;
    head << output;
    head.type = JSON;

    return new json_document(head);
}

vector_t *NeuralNetwork::forwardPropagate(vector_t *x)
{
    vector_t y_n(*x); // copy
    DIGGI_ASSERT(x);
    for (unsigned i = 0; i < layers->size(); i++)
    {
        vector_t y_k;
        for (unsigned j = 0; j < layers->at(i)->size(); j++)
        {
            auto nrn = layers->at(i)->at(j);
            DIGGI_ASSERT(nrn);
            double v_n = weightedSum(nrn->weights, &y_n);
            nrn->output = sigmoid(v_n);
            y_k.push_back(nrn->output);
        }
        y_n = y_k; // copy
    }
    return new vector_t(y_n); // Copy
}
/**
 * @brief train neural network on input matrix
 * 
 * @param X input matrix
 * @param learn_rate volatility, learning rate for each training session
 * @param epochs rounds of training
 * @param output_n dimentionality of output
 * @return vector_t* sum of error squared
 */
vector_t *NeuralNetwork::train(
    matrix_t *X,
    double learn_rate,
    size_t epochs,
    size_t output_n)
{

    auto sqr_sum_error = new vector_t();
    for (unsigned i = 0; i < epochs; i++)
    {
        auto error = 0.0;
        for (unsigned j = 0; j < X->size(); j++)
        {
            auto y_true = vector_t(output_n, 0);
            y_true.at(X->at(j)->back()) = 1;
            auto y_hat = forwardPropagate(X->at(j));
            error = sumOfErrorsSqrd(y_hat, &y_true);
            delete y_hat;
            backwardsPropagate(&y_true);
            updateWeights(X->at(j), learn_rate);
        }
        sqr_sum_error->push_back(error);
    }
    return sqr_sum_error;
}
/**
 * @brief internal method for calucaling sum of errors squared of a ground truth compared to classification.
 * 
 * @param y_hat 
 * @param y 
 * @return double 
 */
double NeuralNetwork::sumOfErrorsSqrd(vector_t *y_hat, vector_t *y)
{
    auto sum = 0.0;
    for (unsigned i = 0; i < y->size(); i++)
    {
        sum += sqr(y->at(i) - y_hat->at(i));
    }
    return sum;
}
/**
 * @brief internal method for square operation on value
 * 
 * @param val 
 * @return double 
 */
double NeuralNetwork::sqr(double val)
{
    return val * val;
}
/**
 * @brief retrieve a weighted sum of vector x.
 * 
 * @param w weights
 * @param x vector x
 * @return double 
 */
double NeuralNetwork::weightedSum(vector_t *w, vector_t *x)
{
    auto sum = w->back();
    for (unsigned i = 0; i < w->size() - 1; i++)
    {
        sum += w->at(i) * x->at(i);
    }
    return sum;
}
/**
 * @brief sigmoid activation function
 * 
 * @param x 
 * @return double 
 */
double NeuralNetwork::sigmoid(double x)
{
    return (1.0 / (1.0 + exp(-x)));
}
/**
 * @brief sigmoid derived activation funciton
 * 
 * @param x 
 * @return double 
 */
double NeuralNetwork::sigmoidMarked(double x)
{
    return (x * (1.0 - x));
}
/**
 * @brief calculate sum of errors for a given layer.
 * 
 * @param index_i 
 * @param index_j 
 * @return double 
 */
double NeuralNetwork::sumLayerError(size_t index_i, size_t index_j)
{
    auto error = 0.0;
    for (unsigned i = 0; i < layers->at(index_i)->size(); i++)
    {
        auto nrn = layers->at(index_i)->at(i);
        error += nrn->weights->at(index_j) * nrn->delta;
    }
    return error;
}
/**
 * @brief make a prediction on a input vector.
 * 
 * @param x 
 * @return size_t 
 */
size_t NeuralNetwork::predict(vector_t *x)
{
    // for(double val: (std::vector<double>)*x){
    //     printf("%f ",val);
    // }
    auto y = forwardPropagate(x);
    // DIGGI_DEBUG_BREAK();
    auto retval = std::distance(y->begin(), std::max_element(y->begin(), y->end()));
    delete y;
    return retval;
}
/**
 * @brief backwards propagation algorithm for output vector y
 * 
 * @param y 
 */
void NeuralNetwork::backwardsPropagate(vector_t *y)
{

    for (int i = (layers->size() - 1); i >= 0; i--)
    {
        auto layer = layers->at(i);
        vector_t err;

        if ((unsigned)i == (layers->size() - 1))
        {
            for (unsigned j = 0; j < layer->size(); j++)
            {
                err.push_back(y->at(j) - layer->at(j)->output);
            }
        }
        else
        {
            for (unsigned j = 0; j < layer->size(); j++)
            {
                err.push_back(sumLayerError(i + 1, j));
            }
        }

        for (unsigned j = 0; j < layer->size(); j++)
        {
            auto nrn = layer->at(j);
            nrn->delta = err[j] * sigmoidMarked(nrn->output);
        }
    }
}

/**
 * @brief update weights
 * 
 * @param x 
 * @param learn 
 */
void NeuralNetwork::updateWeights(vector_t *x, double learn)
{
    vector_t y_next(*x);
    for (unsigned i = 0; i < layers->size(); i++)
    {
        auto layer = layers->at(i);
        auto y = y_next;
        y_next.clear();
        for (unsigned j = 0; j < layer->size(); j++)
        {
            auto nrn = layer->at(j);
            for (unsigned k = 0; k < y.size() - 1; k++)
            {
                nrn->weights->at(k) += (learn * nrn->delta * y[k]);
            }
            nrn->weights->back() += learn * nrn->delta; //bias
            y_next.push_back(nrn->output);
        }
    }
}
