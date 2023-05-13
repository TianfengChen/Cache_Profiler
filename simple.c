/*
Simple papi instrumentation measuring the total cycles of a printf operation.
Install PAPI following the instructions on https://bitbucket.org/icl/papi/wiki/Downloading-and-Installing-PAPI.md
Compile with the following command,
gcc -O2 -Wall -I${PAPI_DIR}/src -o simple simple.c ${PAPI_DIR}/src/libpapi.a
Replace ${PAPI_DIR} with the path of papi in your device,
e.g. for my papi under my home directory, it is ~/papi/src
*/
#include <stdio.h>
#include <papi.h>

int main(int argc, char **argv)
{
    int retval;

    // Initialize PAPI
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT)
    {
        fprintf(stderr, "Errofasdfasdfr initializing PAPI! %s\n",
                PAPI_strerror(retval));
        return 0;
    }

    // Create an event
    int eventset = PAPI_NULL;

    retval = PAPI_create_eventset(&eventset);
    if (retval != PAPI_OK)
    {
        fprintf(stderr, "Error creating eventset! %s\n",
                PAPI_strerror(retval));
    }

    // Add an event, PAPI_TOT_CYC
    // Multiple events can be addded and measure simultaneously.
    // There is a hardware limit of simultaneous events. Use multiplex if exceed.
    retval = PAPI_add_named_event(eventset, "PAPI_TOT_INS");
    if (retval != PAPI_OK)
    {
        fprintf(stderr, "Error adding PAPI_TOT_CYC: %s\n",
                PAPI_strerror(retval));
    }
    retval = PAPI_add_named_event(eventset, "PAPI_LD_INS");
    if (retval != PAPI_OK)
    {
        fprintf(stderr, "Error adding PAPI_TOT_CYC: %s\n",
                PAPI_strerror(retval));
    }
    retval = PAPI_add_named_event(eventset, "PAPI_SR_INS");
    if (retval != PAPI_OK)
    {
        fprintf(stderr, "Error adding PAPI_TOT_CYC: %s\n",
                PAPI_strerror(retval));
    }

    // Start Event
    long long count[3];

    PAPI_reset(eventset);
    retval = PAPI_start(eventset);
    if (retval != PAPI_OK)
    {
        fprintf(stderr, "Error starting CUDA: %s\n",
                PAPI_strerror(retval));
    }

    // ---  START of measured code snippet ---
    printf("sadfasdf");
    // ---  END of measured code snippet   ---

    retval = PAPI_stop(eventset, count);
    if (retval != PAPI_OK)
    {
        fprintf(stderr, "Error stopping:  %s\n",
                PAPI_strerror(retval));
    }
    else
    {
        printf("Measured %lld cycles\n", count[0]);
        printf("Measured %lld cycles\n", count[1]);
        printf("Measured %lld cycles\n", count[2]);
    }
}