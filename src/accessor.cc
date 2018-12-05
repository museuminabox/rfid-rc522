#include <node_api.h>
//#include <v8.h>
#include <unistd.h>
#include "rfid.h"
#include "rc522.h"
#include "bcm2835.h"

#define DEFAULT_SPI_SPEED 5000L

uint8_t initRfidReader(void);

uint16_t CType = 0;
uint8_t serialNumber[10];
uint8_t serialNumberLength = 0;
uint8_t noTagFoundCount = 0;
char rfidChipSerialNumber[23];
char rfidChipSerialNumberRecentlyDetected[23];
char *p;
int loopCounter;

#if 0
using namespace v8;

void RunCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    Local<Function> callback = Local<Function>::Cast(args[0]);
    const unsigned argc = 1;

    InitRc522();

    for (;;) {
        statusRfidReader = find_tag(&CType);
        if (statusRfidReader == TAG_NOTAG) {

            // The status that no tag is found is sometimes set even when a tag is within reach of the tag reader
            // to prevent that the reset is performed the no tag event has to take place multiple times (ger: entrprellen)
            if (noTagFoundCount > 2) {
                // Sets the content of the array 'rfidChipSerialNumberRecentlyDetected' back to zero
                memset(&rfidChipSerialNumberRecentlyDetected[0], 0, sizeof (rfidChipSerialNumberRecentlyDetected));
                noTagFoundCount = 0;
            } else {
                noTagFoundCount++;
            }

            usleep(200000);
            continue;
        } else if (statusRfidReader != TAG_OK && statusRfidReader != TAG_COLLISION) {
            continue;
        }

        if (select_tag_sn(serialNumber, &serialNumberLength) != TAG_OK) {
            continue;
        }

        // Is a successful detected, the counter will be set to zero
        noTagFoundCount = 0;

        p = rfidChipSerialNumber;
        for (loopCounter = 0; loopCounter < serialNumberLength; loopCounter++) {
            sprintf(p, "%02x", serialNumber[loopCounter]);
            p += 2;
        }

        // Only when the serial number of the currently detected tag differs from the
        // recently detected tag the callback will be executed with the serial number
        if (strcmp(rfidChipSerialNumberRecentlyDetected, rfidChipSerialNumber) != 0) {
            Local<Value> argv[argc] = {
                String::NewFromUtf8(isolate, &rfidChipSerialNumber[0])
            };

            callback->Call(isolate->GetCurrentContext()->Global(), argc, argv);
        }

        // Preserves the current detected serial number, so that it can be used
        // for future evaluations
        strcpy(rfidChipSerialNumberRecentlyDetected, rfidChipSerialNumber);

        *(p++) = 0;
    }

    bcm2835_spi_end();
    bcm2835_close();
}
#endif

napi_value checkForTag(napi_env env, napi_callback_info args) {
    char statusRfidReader = TAG_NOTAG;
    uint8_t serialNumber[10];
    uint8_t serialNumberLength = 0;
    uint8_t noTagFoundCount = 0;
    char rfidChipSerialNumber[23];
    char rfidChipSerialNumberRecentlyDetected[23];
    napi_value ret;
    napi_status err;

    // The status that no tag is found is sometimes set even when a tag is within reach of the tag reader
    // to prevent that the reset is performed the no tag event has to take place multiple times (ger: entrprellen)
    while ((noTagFoundCount < 3) && (statusRfidReader == TAG_NOTAG))
    {
        statusRfidReader = find_tag(&CType);
        if (statusRfidReader == TAG_NOTAG) {
            noTagFoundCount++;
            usleep(200000);
        }
    }

    if (statusRfidReader == TAG_OK || statusRfidReader == TAG_COLLISION) {
        
        uint8_t sak;
	    statusRfidReader = select_tag_sn(serialNumber, &serialNumberLength, &sak);
        if (statusRfidReader != TAG_OK) {
            // Return error
            err = napi_create_int32(env, statusRfidReader, &ret);
            if (err != napi_ok) {
                // Couldn't create return value...
                napi_throw_error(env, "General error", "Failed to create error return");
                return nullptr;
            }
            return ret;
        } else {
            // Convert serial number to a string
            p = rfidChipSerialNumber;
            for (loopCounter = 0; loopCounter < serialNumberLength; loopCounter++) {
                sprintf(p, "%02x", serialNumber[loopCounter]);
                p += 2;
            }

            // Convert string to a Javascript string to return
            err = napi_create_string_latin1(env, rfidChipSerialNumber, strlen(rfidChipSerialNumber), &ret);
            if (err == napi_ok) {
                return ret;
            } else {
                // Error
            	// FIXME We should return the error code really
                napi_throw_error(env, "General error", "Error creating string");
                return nullptr;
            }
        }
    } else if (statusRfidReader == TAG_NOTAG) {
        // Tag not found
        return nullptr;
    } else {
        // Some sort of error
	    // FIXME We should return the error code really
        err = napi_create_int32(env, statusRfidReader, &ret);
        if (err != napi_ok) {
            // Couldn't create return value...
            napi_throw_error(env, "General error", "Failed to create error return");
            return nullptr;
        }
        return ret;
    }
}

napi_value readPage(napi_env env, napi_callback_info args) {
    size_t arg_count = 2;
    napi_value argv[2];
    napi_value thisArg = NULL;
    void* data = NULL;
    napi_status status = napi_ok;

    // Get the arguments info from the caller
    status = napi_get_cb_info(env, args, &arg_count, argv, &thisArg, &data);
    if (status != napi_ok) {
        // Couldn't get info...
        napi_throw_error(env, "General error", "Failed to retrieve NAPI arguments");
        return nullptr;
    }

    // Check the number of arguments passed.
    if (arg_count != 2) {
        // Throw an Error that is passed back to JavaScript
        napi_throw_type_error(env, "Invalid", "Wrong number of arguments");
        return nullptr;
    }

    // Check the argument types
    // FIXME We should use napi_typeof to check this and argv[1]
#if 0
    if (!argv[0]->IsNumber()) {
        napi_throw_type_error(env, "Number expected", "Wrong arguments");
        return nullptr;
    }
#endif

    // Which page have we been asked for
    int32_t pageNumber;
    status = napi_get_value_int32(env, argv[0], &pageNumber);
    if (status != napi_ok) {
        napi_throw_error(env, "General error", "Failed to read page number");
        return nullptr;
    }

    // Fetch it
    uint8_t buf[16]; // We only want 4 bytes of this, but...
    printf("Reading page %d\n", pageNumber);
    char err = TAG_ERR;
    int tries = 0;
    // FIXME We try this up to 8 times to try to make it more reliable, but
    // FIXME it doesn't seem to make much difference, still fails with err == 3 too often
    while (tries++ < 8 && err != TAG_OK) {
        printf(".");
        err = PcdRead(pageNumber, buf);
        if (err != TAG_OK) {
            usleep(200000);
        }
    }
    printf("err: %d\n", err);
    for (int i =0; i < 16; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");

    napi_value ret;
    void* ret_data;
    if (err == TAG_OK) {
        // We've got valid data, return the first 4 bytes
        status = napi_create_buffer_copy(env, 4, buf, &ret_data, &ret);
        if (status != napi_ok) {
            // Couldn't create return value...
            napi_throw_error(env, "General error", "Failed to create return buffer");
            return nullptr;
        }
    } else {
        // Return the error
        status = napi_create_int32(env, err, &ret);
        if (status != napi_ok) {
            // Couldn't create return value...
            napi_throw_error(env, "General error", "Failed to create return value");
            return nullptr;
        }
    }

    napi_value* ret_argv = &ret;
    size_t ret_argc = 1;
    napi_value callback_ret;
    status = napi_call_function(env, thisArg, argv[1], ret_argc, ret_argv, &callback_ret);
    if (status != napi_ok) {
        // Couldn't call the callback
        napi_throw_error(env, "General error", "Failed to call callback");
        return nullptr;
    }

    return nullptr;
}

napi_value Init(napi_env env, napi_value exports) {
    napi_status status;
    napi_value fn;

    initRfidReader();

    status = napi_create_function(env, nullptr, 0, readPage, nullptr, &fn);
    if (status != napi_ok) return nullptr;

    status = napi_set_named_property(env, exports, "readPage", fn);
    if (status != napi_ok) return nullptr;

    status = napi_create_function(env, nullptr, 0, checkForTag, nullptr, &fn);
    if (status != napi_ok) return nullptr;

    status = napi_set_named_property(env, exports, "checkForTag", fn);
    if (status != napi_ok) return nullptr;
    return exports;
}

uint8_t initRfidReader(void) {
    uint16_t sp;

    sp = (uint16_t) (250000L / DEFAULT_SPI_SPEED);
    if (!bcm2835_init()) {
        return 1;
    }

    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST); // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); // The default
    bcm2835_spi_setClockDivider(sp); // The default
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0); // The default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); // the default

    // set up the RFID reader
    InitRc522();

    return 0;
}

// FIXME Should we set up an napi_add_env_cleanup_hook to tidy up any resources if Node exits?

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
