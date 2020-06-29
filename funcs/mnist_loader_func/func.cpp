#include "runtime/func.h"

#include "DiggiAssert.h"
#include "runtime/DiggiAPI.h"
#include "storage/DBClient.h"
#include "messaging/Util.h"
#include "hex_lib.h"
/*Com func*/
#define MNIST_LOAD_COMPLETE (msg_type_t)12345

std::string imagetableName = "ImageTable";
std::string labletableName = "LableTable";
std::string testimagetableName = "testImageTable";
std::string testlabletableName = "testLableTable";
std::string testimagetableCreate = "DROP TABLE IF EXISTS " +
                                   testimagetableName + ";"
                                                        "CREATE TABLE " +
                                   testimagetableName + " ("
                                                        "ID INT PRIMARY KEY     NOT NULL,"
                                                        "IMAGEDATA      BLOB    NOT NULL);";
std::string testlabeltableCreate = "DROP TABLE IF EXISTS " +
                                   testlabletableName + ";"
                                                        "CREATE TABLE " +
                                   testlabletableName + " ("
                                                        "ID INT PRIMARY KEY     NOT NULL,"
                                                        "LABEL      INT    NOT NULL);";
std::string imagetableCreate = "DROP TABLE IF EXISTS " +
                               imagetableName + ";"
                                                "CREATE TABLE " +
                               imagetableName + " ("
                                                "ID INT PRIMARY KEY     NOT NULL,"
                                                "IMAGEDATA      BLOB    NOT NULL);";
std::string labeltableCreate = "DROP TABLE IF EXISTS " +
                               labletableName + ";"
                                                "CREATE TABLE " +
                               labletableName + " ("
                                                "ID INT PRIMARY KEY     NOT NULL,"
                                                "LABEL      INT    NOT NULL);";

void parse_images(const char *path, DBClient *cli, DiggiAPI *api, std::string table, int sample_count)
{
    mode_t mode = S_IRWXU;
    int oflags = O_RDWR;
    int input_file_pointer = __real_open(path, oflags, mode);

    char magic_number_bytes[4];
    __real_read(input_file_pointer, magic_number_bytes, 4);
    // If MSB is first then magic_number_bytes will be 0x00000803
    api->GetLogObject()->Log(LRELEASE, "magic number: %d\n", hex_array_to_int(magic_number_bytes, 4));
    char number_of_images_bytes[4];
    __real_read(input_file_pointer, number_of_images_bytes, 4);
    int32_t number_of_images = hex_array_to_int(number_of_images_bytes, 4);
    api->GetLogObject()->Log(LRELEASE, "number of images: %d\n", number_of_images);

    int32_t num_rows = 0, num_cols = 0;
    char num_row_cols[4];
    __real_read(input_file_pointer, num_row_cols, 4);
    num_rows = hex_array_to_int(num_row_cols, 4);
    __real_read(input_file_pointer, num_row_cols, 4);
    num_cols = hex_array_to_int(num_row_cols, 4);
    api->GetLogObject()->Log(LRELEASE, "pixel rows: %d and pixel columns: %d\n", num_rows, num_cols);

    //   FILE* output_file_pointer = fopen(argv[2], "w");
    //   CHECK_NOTNULL(output_file_pointer);

    int32_t num_pixles_in_image = num_cols * num_rows;
    char images_pixels_bytes[num_pixles_in_image];
    int images_done = 0;
    while (images_done++ < number_of_images)
    {
        DIGGI_ASSERT(__real_read(input_file_pointer, images_pixels_bytes, num_pixles_in_image) > 0);
        auto sql = "INSERT INTO " + table + " VALUES(" + std::to_string(images_done - 1) + ", ?);";
        // Utils::print_byte_array(images_pixels_bytes, num_pixles_in_image);
        cli->executeBlob(sql.c_str(), images_pixels_bytes, num_pixles_in_image);
        if (images_done >= sample_count)
        {
            break;
        }
    }

    __real_close(input_file_pointer);
}

void parse_label(const char *path, DBClient *cli, DiggiAPI *api, std::string table, int samplecount)
{
    mode_t mode = S_IRWXU;
    int oflags = O_RDWR;
    int input_file_pointer = __real_open(path, oflags, mode);

    char magic_number_bytes[4];
    __real_read(input_file_pointer, magic_number_bytes, 4);
    // If MSB is first then magic_number_bytes will be 0x00000801

    api->GetLogObject()->Log(LRELEASE, "magic number: %d\n", hex_array_to_int(magic_number_bytes, 4));

    char number_of_images_bytes[4];
    __real_read(input_file_pointer, number_of_images_bytes, 4);
    api->GetLogObject()->Log(LRELEASE, "number of labels: %d\n", hex_array_to_int(number_of_images_bytes, 4));

    // Open a file for dumping the labels.

    char label_byte;
    int images_done = 0;
    while (__real_read(input_file_pointer, &label_byte, 1))
    {

        auto sql = "INSERT INTO " + table + " VALUES(" + std::to_string(images_done) + "," + std::to_string(hex_char_to_int(label_byte)) + ");";
        cli->execute(sql.c_str());

        images_done++;
        if (images_done >= samplecount)
        {
            break;
        }
    }

    __real_close(input_file_pointer);
}
void func_init(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
}
void func_start(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    DiggiAPI *a_cont = (DiggiAPI *)ctx;
    DBClient *cli = new DBClient(a_cont->GetMessageManager(), a_cont->GetThreadPool(), CLEARTEXT);
    cli->connect(a_cont->GetFuncConfig()["load-target-db-func"].value.tostring());
    int sample_count = atoi(a_cont->GetFuncConfig()["train-sample-count"].value.tostring().c_str());
    int test_sample_count = atoi(a_cont->GetFuncConfig()["test-sample-count"].value.tostring().c_str());

    a_cont->GetLogObject()->Log(LRELEASE, "Creating training image table\n");

    cli->execute(imagetableCreate.c_str());
    a_cont->GetLogObject()->Log(LRELEASE, "Creating training lable table\n");

    cli->execute(labeltableCreate.c_str());
    a_cont->GetLogObject()->Log(LRELEASE, "Loading training images into DB\n");
    parse_images(a_cont->GetFuncConfig()["train-image-path"].value.tostring().c_str(), cli, a_cont, imagetableName, sample_count);
    a_cont->GetLogObject()->Log(LRELEASE, "Loading training lables into DB\n");
    parse_label(a_cont->GetFuncConfig()["train-label-path"].value.tostring().c_str(), cli, a_cont, labletableName, sample_count);
    a_cont->GetLogObject()->Log(LRELEASE, "Done training loading..\n");

    a_cont->GetLogObject()->Log(LRELEASE, "Creating test image table\n");

    cli->execute(testimagetableCreate.c_str());
    a_cont->GetLogObject()->Log(LRELEASE, "Creating test lable table\n");

    cli->execute(testlabeltableCreate.c_str());
    a_cont->GetLogObject()->Log(LRELEASE, "Loading test images into DB\n");
    parse_images(a_cont->GetFuncConfig()["test-image-path"].value.tostring().c_str(), cli, a_cont, testimagetableName, test_sample_count);
    a_cont->GetLogObject()->Log(LRELEASE, "Loading test lables into DB\n");
    parse_label(a_cont->GetFuncConfig()["test-label-path"].value.tostring().c_str(), cli, a_cont, testlabletableName, test_sample_count);
    a_cont->GetLogObject()->Log(LRELEASE, "Done test loading..\n");
    delete cli;
    auto msg = a_cont->GetMessageManager()->allocateMessage("neural_network_train_func", 1024, REGULAR, CLEARTEXT);
    msg->type = MNIST_LOAD_COMPLETE;
    a_cont->GetMessageManager()->Send(msg, nullptr, nullptr);

    //do nothing
}

/*Simple loopback test*/

void func_stop(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    a_cont->GetLogObject()->Log(LRELEASE, "Stopping MNIST loader func\n");
}
