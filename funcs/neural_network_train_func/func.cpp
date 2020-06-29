#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "DiggiAssert.h"
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "NeuralNetwork.h"
#include "misc.h"
#include "hex_lib.h"
#include <stdlib.h> /* strtod */
#include "storage/DBClient.h"
#include "messaging/Util.h"
#include "telemetry.h"
static NeuralNetwork *nn = nullptr;
std::string imagetableName = "ImageTable";
std::string labletableName = "LableTable";
std::string testimagetableName = "testImageTable";
std::string testlabletableName = "testLableTable";
size_t next_range = 0;
size_t output_layers = 0;
size_t hidden_layers = 0;
size_t epoch = 0;
size_t epoch_num = 0;
double learning_rate = 0.0;
/*do not yield here, another thread entirely*/
#define MATRIX_BATCH_SIZE 40
#define MNIST_LOAD_COMPLETE (msg_type_t)12345

void func_init(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    a_cont->GetLogObject()->Log(LRELEASE, "Initializing NeuralNetwork func\n");
    output_layers = (size_t)atoi(a_cont->GetFuncConfig()["output-layers"].value.tostring().c_str());
    hidden_layers = (size_t)atoi(a_cont->GetFuncConfig()["hidden-layers"].value.tostring().c_str());
    epoch_num = (size_t)atoi(a_cont->GetFuncConfig()["epochs"].value.tostring().c_str());
    learning_rate = (double)atof(a_cont->GetFuncConfig()["learning-rate"].value.tostring().c_str());
    int syscall_interpose = (a_cont->GetFuncConfig()["syscall-interposition"].value == "1");
    iostub_setcontext(a_cont, true);
    set_syscall_interposition(syscall_interpose);
#ifndef DIGGI_ENCLAVE
    telemetry_init();
#endif
}

matrix_t *get_matrix(DiggiAPI *api, size_t size, size_t offset, std::string image_table_name, std::string lable_table_name)
{
    auto dbcli = new DBClient(api->GetMessageManager(), api->GetThreadPool(), ENCRYPTED);
    dbcli->connect(api->GetFuncConfig()["data-source"].value.tostring());
    dbcli->execute("SELECT * FROM %s ORDER BY ID LIMIT %lu OFFSET %lu;", image_table_name.c_str(), size, offset);
    auto images = dbcli->fetchall();
    // dbcli->freeDBResults();

    dbcli->execute("SELECT * FROM %s ORDER BY ID LIMIT %lu OFFSET %lu;", lable_table_name.c_str(), size, offset);
    auto lables = dbcli->fetchall();
    // dbcli->freeDBResults();
    auto mtrx = new matrix_t();
    for (size_t i = 0; i < images.size(); i++)
    {
        auto putrow = new vector_t();
        auto test = images[i].getInt(0);
        DIGGI_ASSERT((int)(i + offset) == test);
        auto stringfield = images[i].getString(1);
        // Utils::print_byte_array((void *)stringfield.c_str(), stringfield.size());

        for (size_t x = 0; x < stringfield.size(); x++)
        {
            auto f = hex_char_to_int(stringfield[x]);
            double normalized = static_cast<double>(f) / static_cast<double>(255);
            putrow->push_back(normalized);
        }
        DIGGI_ASSERT((int)(i + offset) == lables[i].getInt(0));
        // printf("lables %f\n", static_cast<double>(lables[i].getInt(1)));
        putrow->push_back(static_cast<double>(lables[i].getInt(1)));
        mtrx->push_back(putrow);
    }
    dbcli->freeDBResults();

    delete dbcli;
    return mtrx;
}
int hit = 0;
int miss = 0;
void test_network(void *ptr, int status)
{
    auto matrix = (matrix_t *)ptr;
    telemetry_capture("--predict begin--");

    for (unsigned i = 0; i < matrix->size(); i++)
    {
        auto sample = matrix->at(i);
        auto ground_truth = sample->back();
        sample->pop_back();
        auto predict = nn->predict(sample);
        telemetry_capture("predict");
        // printf("sample_size = %lu, predict = %lu, ground truth = %f\n", sample->size(), predict, ground_truth);
        if (predict == ground_truth)
        {
            hit++;
        }
        else
        {
            miss++;
        }
    }

    for (auto vec : (std::vector<vector_t *>)*matrix)
    {
        delete vec;
    }
    delete matrix;
}

double square_error = 0.0;
double total = 0.0;
void compute_matrix_chunk(void *ptr, int status)
{
    auto matrix = (matrix_t *)ptr;
    total = 0.0;
    // GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject()->Log(LRELEASE, "Training\n");
    telemetry_capture("train begin");
    auto square = nn->train(matrix, learning_rate, 1, output_layers);
    telemetry_capture("train");

    for (double val : (std::vector<double>)*square)
    {
        total += val;
    }
    square_error = total / square->size();
    delete square;
    test_network(matrix, 1);
}
bool train = true;

void fetch_matrix_chunk(void *ctx, int status)
{
    auto a_cont = (DiggiAPI *)ctx;
    auto fetch_range = next_range;
    next_range += MATRIX_BATCH_SIZE;
    auto matrix = (train)
                      ? get_matrix(a_cont, MATRIX_BATCH_SIZE, fetch_range, imagetableName, labletableName)
                      : get_matrix(a_cont, MATRIX_BATCH_SIZE, fetch_range, testimagetableName, testlabletableName);
    // a_cont->GetLogObject()->Log(LRELEASE, "fetch matrix size=%lu\n", matrix->size());

    if (matrix->size() == 0)
    {
        epoch++;
        next_range = 0;
        if (epoch < epoch_num && train)
        {
            a_cont->GetLogObject()->Log(LRELEASE, "Done Processing input for epoch=%lu, learnrate=%f square_error=%lf\n", epoch, learning_rate, square_error);
            double perc_loss = (double)miss / (double)(hit + miss);
            a_cont->GetLogObject()->Log(LRELEASE, "Test on self data:  hits=%d misses %d, percent_loss=%f\n", hit, miss, perc_loss);
            hit = 0;
            miss = 0;
            a_cont->GetThreadPool()->Schedule(fetch_matrix_chunk, a_cont, __PRETTY_FUNCTION__);
        }
        else
        {
            if (!train)
            {
                double perc_loss = (double)miss / (double)(hit + miss);
                a_cont->GetLogObject()->Log(LRELEASE, "Done Processing input for epoch=%lu, learnrate=%f square_error=%lf\n", epoch, learning_rate, square_error);

                a_cont->GetLogObject()->Log(LRELEASE, "Test complete: hits=%d misses %d, percent_loss=%f\n", hit, miss, perc_loss);
                delete nn;
#ifndef DIGGI_ENCLAVE
                telemetry_write();
#endif
                a_cont->GetSignalHandler()->Stopfunc();
            }
            else
            {
                a_cont->GetLogObject()->Log(LRELEASE, "Train complete\n");
                epoch = 0;
                hit = 0;
                miss = 0;
                a_cont->GetThreadPool()->Schedule(fetch_matrix_chunk, a_cont, __PRETTY_FUNCTION__);
            }
            train = false;
        }
        for (auto vec : (std::vector<vector_t *>)*matrix)
        {
            delete vec;
        }
        delete matrix;
        return;
    }
    if (fetch_range == 0 && epoch == 0 && train)
    {
        a_cont->GetLogObject()->Log(LRELEASE, "Creating neural net input_layers=%lu, hidden_layers=%lu, output_layers=%lu\n", matrix->at(0)->size() - 1, hidden_layers, output_layers);

        DIGGI_ASSERT(output_layers > 0);
        DIGGI_ASSERT(matrix[0].size() > 0);
        nn = new NeuralNetwork(matrix->at(0)->size() - 1, hidden_layers, output_layers);
    }
    if (train)
    {
        a_cont->GetThreadPool()->Schedule(compute_matrix_chunk, matrix, __PRETTY_FUNCTION__);
    }
    else
    {
        a_cont->GetThreadPool()->Schedule(test_network, matrix, __PRETTY_FUNCTION__);
    }
    a_cont->GetThreadPool()->Schedule(fetch_matrix_chunk, a_cont, __PRETTY_FUNCTION__);
}

void start_training(void *ctx, int status)
{
    auto resp = (msg_async_response_t *)ctx;
    auto a_cont = (DiggiAPI *)resp->context;
    a_cont->GetLogObject()->Log(LRELEASE, "Recieved done signal from loader func\n");

    a_cont->GetThreadPool()->Schedule(fetch_matrix_chunk, a_cont, __PRETTY_FUNCTION__);
}
void func_start(void *ctx, int status)
{
    auto a_cont = (DiggiAPI *)ctx;
    DIGGI_ASSERT(a_cont);
    a_cont->GetLogObject()->Log(LRELEASE, "Starting NeuralNetwork func\n");
    a_cont->GetMessageManager()->registerTypeCallback(start_training, MNIST_LOAD_COMPLETE, a_cont);
}

void func_stop(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    a_cont->GetLogObject()->Log("Stopping NeuralNetwork func\n");
}
