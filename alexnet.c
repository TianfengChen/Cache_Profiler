/////////////////////////////////////////////////
// alexnet.c
// basically a simple CNN 
// kindly contributed by group 7 Fall 2019
//   - Jielun Tan, 12/2019
/////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <papi.h>
#include <stdio.h>

#define TEST_ITR 300
#define WINDOW_SIZE 1000
#define SIZE_F_COUNT 2

#include "tj_malloc.h"
int total = 0; /* total overflows */
int EventSet = PAPI_NULL;
long long f_count[SIZE_F_COUNT];
long long count[2];
int is_profling = 0;

int handler_es = PAPI_NULL;
//int interval = 100;
int interval = 20;
float lower_bound = 0.9;
float upper_bound = 1.1;
float scale = 1.5;
//int min_interval = 100;
//int max_interval = 100000;
int min_interval = 2;
int max_interval = 2;
int round = 0;
long long total_miss = 0;

unsigned int lfsr = 0xACE1u;
unsigned period = 0;
char s[16+1];

void handle_error(int retval)
{
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
}

void handler(int EventSet, void *address, long_long overflow_vector, void *context)
{
    /*fprintf(stderr, "handler(%d) Overflow at %p! vector=0x%llx\n",
            EventSet, address, overflow_vector);*/

    int retval;
    round++;

    // printf("count: %d %d %d\n ", count[0], count[1], count[2]);
    if (round == interval && is_profling == 0)
    {
        retval = PAPI_stop(EventSet, count);
        if (retval != PAPI_OK)
            fprintf(stderr, "Error stopping eventset! %s\n",
                    PAPI_strerror(retval));

        retval = PAPI_add_named_event(EventSet, "PAPI_L1_DCM");
        if (retval != PAPI_OK)
        {
            fprintf(stderr, "Error adding PAPI_L1_CYC: %s\n",
                    PAPI_strerror(retval));
            exit(0);
        }

        round = 0;
        is_profling = 1;

        retval = PAPI_start(EventSet);
        if (retval != PAPI_OK)
            fprintf(stderr, "Error starting eventset! %s\n",
                    PAPI_strerror(retval));
    }
    else if (is_profling == 1)
    {
        retval = PAPI_stop(EventSet, count);
        if (retval != PAPI_OK)
            fprintf(stderr, "Error stopping eventset! %s\n",
                    PAPI_strerror(retval));
        printf("misses in window: %d, interval size: %d\n ", count[1], interval);
        total_miss += interval * count[1];

        int current_window_miss = count[1];
        int flag = 0;
        for (int i = 0; i < SIZE_F_COUNT; i++) {
            flag += (float)current_window_miss > ((float) f_count[i]) * upper_bound || (float)current_window_miss < ((float) f_count[i]) * lower_bound;
        }
        if (flag == 0) {
            interval = interval * scale;
        }
        else{
            interval = interval / scale;
        }
        if (interval > max_interval) {
            interval = max_interval;
        }
        if (interval < min_interval) {
            interval = min_interval;
        }

        for (int i = 0; i < SIZE_F_COUNT-1; i++) {
            f_count[i] = f_count[i+1];
        }
        f_count[SIZE_F_COUNT-1] = current_window_miss;

        retval = PAPI_remove_named_event(EventSet, "PAPI_L1_DCM");
        if (retval != PAPI_OK)
        {
            fprintf(stderr, "Error removing PAPI_TOT_CYC: %s\n",
                    PAPI_strerror(retval));
            exit(0);
        }

        is_profling = 0;

        retval = PAPI_start(EventSet);
        if (retval != PAPI_OK)
            fprintf(stderr, "Error starting eventset! %s\n",
                    PAPI_strerror(retval));
    }
}

int random_gen(){
    unsigned lsb = lfsr & 1;

    lfsr >>= 1;

    if (lsb == 1)
        lfsr ^= 0xB400u;

    return lfsr;
}

int relu_af(int in_af){
	if(in_af < 0)
		return 0;

	return in_af;
}

void fc_layer(int weights[5][5], int biasses[5], int inputs[5], int outputs[5], int input_num, int output_num, int af_num){
	int i;
	int j;
	for (i = 0; i < output_num; i++) {
		outputs[i] = biasses[i];
		for (j = 0; j < input_num; j++) {
			outputs[i] += weights[i][j] * inputs[j];
		}
        switch (af_num) {
            case 0:
                outputs[i] = relu_af(outputs[i]);
                break;
            case 1:
                outputs[i] = relu_af(outputs[i]);
                break;
        }
	}
}

void fc_input_generator(int inputs[5], int input_num){
	int i;
    for (i = 0; i < input_num; i++) {
        inputs[i] = (random_gen()&15) - 5;
    }
}

void fc_weight_generator(int weights[5][5], int biasses[5], int input_num, int output_num){
	int i;
	int j;
	for (i = 0; i < output_num; i++) {
        biasses[i] = (random_gen()&15) - 5;
        for (j = 0; j < input_num; j++) {
            weights[i][j] = (random_gen()&15) - 5;
        }
    }
}

int fc_soft_max(int features[5], int feature_num){
	int i;
	int max = 0;
    for (i = 1; i < feature_num; i++) {
        if(features[max] < features[i])
            max = i;
    }
    return max;
}

void cnn_layer(int weights[5][5][5][5], int biasses[5], int inputs[5][5][5], int outputs[5][5][5], int input_channel, int output_channel, int input_size, int kernel_size, int stride, int zero_pad, int af_num){
    int output_size = ((input_size + (2 * zero_pad) - kernel_size) >> stride) + 1;
	int o_ch_itr, o_r_itr, o_c_itr, i_ch_itr, k_r_itr, k_c_itr;
	for (o_ch_itr = 0; o_ch_itr < output_channel; o_ch_itr++) {
        for (o_r_itr = 0; o_r_itr < output_size; o_r_itr++) {
            for (o_c_itr = 0; o_c_itr < output_size; o_c_itr++) {
                outputs[o_ch_itr][o_r_itr][o_c_itr] = biasses[o_ch_itr];
        		for (i_ch_itr = 0; i_ch_itr < input_channel; i_ch_itr++) {
                    for (k_r_itr = 0; k_r_itr < kernel_size; k_r_itr++) {
                        for (k_c_itr = 0; k_c_itr < kernel_size; k_c_itr++) {
                            outputs[o_ch_itr][o_r_itr][o_c_itr] += (((stride*o_r_itr)+k_r_itr-zero_pad) < 0) || (((stride*o_c_itr)+k_c_itr-zero_pad) < 0) || (((stride*o_r_itr)+k_r_itr-zero_pad) >= input_size) || (((stride*o_c_itr)+k_c_itr-zero_pad) >= input_size) ? 0 : inputs[i_ch_itr][(stride*o_r_itr)+k_r_itr-zero_pad][(stride*o_c_itr)+k_c_itr-zero_pad] * weights[o_ch_itr][i_ch_itr][k_r_itr][k_c_itr];
                		}
            		}
        		}
                switch (af_num) {
                    case 0:
                        outputs[o_ch_itr][o_r_itr][o_c_itr] = relu_af(outputs[o_ch_itr][o_r_itr][o_c_itr]);
                        break;
                    case 1:
                        outputs[o_ch_itr][o_r_itr][o_c_itr] = relu_af(outputs[o_ch_itr][o_r_itr][o_c_itr]);
                        break;
                }
    		}
		}
	}
}

void cnn_pool(int inputs[5][5][5], int outputs[5][5][5], int feature_channel, int input_size, int kernel_size, int stride, int zero_pad, int pool_num){
    int output_size = ((input_size + (2 * zero_pad) - kernel_size) >> stride) + 1;
    int new_candidate;
	int ch_itr, o_r_itr, o_c_itr, k_r_itr, k_c_itr;
    for (ch_itr = 0; ch_itr < feature_channel; ch_itr++) {
        for (o_r_itr = 0; o_r_itr < output_size; o_r_itr++) {
            for (o_c_itr = 0; o_c_itr < output_size; o_c_itr++) {
                switch (pool_num) {
                    case 0:
                        outputs[ch_itr][o_r_itr][o_c_itr] = inputs[ch_itr][stride*o_r_itr][stride*o_c_itr];
                        break;
                    case 1:
                        outputs[ch_itr][o_r_itr][o_c_itr] = 0;
                        break;
                }
                for (k_r_itr = 0; k_r_itr < kernel_size; k_r_itr++) {
                    for (k_c_itr = 0; k_c_itr < kernel_size; k_c_itr++) {
                        new_candidate = inputs[ch_itr][(stride*o_r_itr)+k_r_itr][(stride*o_c_itr)+k_c_itr];
                        switch (pool_num) {
                            case 0:
                                outputs[ch_itr][o_r_itr][o_c_itr] = (outputs[ch_itr][o_r_itr][o_c_itr] < new_candidate) ? new_candidate : outputs[ch_itr][o_r_itr][o_c_itr];
                                break;
                            case 1:
                                outputs[ch_itr][o_r_itr][o_c_itr] += new_candidate;
                                break;
                        }
                    }
        		}
                switch (pool_num) {
                    case 1:
                        outputs[ch_itr][o_r_itr][o_c_itr] = outputs[ch_itr][o_r_itr][o_c_itr] - (kernel_size * kernel_size);
                        break;
                }
    		}
		}
	}
}

void cnn_input_generator(int inputs[5][5][5], int input_channel, int input_size){
	int i_ch_itr, i_r_itr, i_c_itr;
	for (i_ch_itr = 0; i_ch_itr < input_channel; i_ch_itr++) {
        for (i_r_itr = 0; i_r_itr < input_size; i_r_itr++) {
            for (i_c_itr = 0; i_c_itr < input_size; i_c_itr++) {
                inputs[i_ch_itr][i_r_itr][i_c_itr] = (random_gen()&15) - 5;
            }
        }
    }
}

void cnn_weight_generator(int weights[5][5][5][5], int biasses[5], int input_channel, int output_channel, int kernel_size){
	int o_ch_itr, i_ch_itr, k_r_itr, k_c_itr;
	for (o_ch_itr = 0; o_ch_itr < output_channel; o_ch_itr++) {
        biasses[o_ch_itr] = (random_gen()&15) - 5;
        for (i_ch_itr = 0; i_ch_itr < input_channel; i_ch_itr++) {
            for (k_r_itr = 0; k_r_itr < kernel_size; k_r_itr++) {
                for (k_c_itr = 0; k_c_itr < kernel_size; k_c_itr++) {
                    weights[o_ch_itr][i_ch_itr][k_r_itr][k_c_itr] = (random_gen()&15) - 5;
                }
            }
        }
    }
}

void cnn_to_fc(int cnn_feature[5][5][5], int cnn_feature_channel, int cnn_feature_size, int fc_feature[5]){
	int ch_itr, r_itr, c_itr;
	for (ch_itr = 0; ch_itr < cnn_feature_channel; ch_itr++) {
        for (r_itr = 0; r_itr < cnn_feature_size; r_itr++) {
            for (c_itr = 0; c_itr < cnn_feature_size; c_itr++) {
                fc_feature[(ch_itr*cnn_feature_size*cnn_feature_size)+(r_itr*cnn_feature_size)+c_itr] = cnn_feature[ch_itr][r_itr][c_itr];
            }
        }
    }
}


int main(){
    int retval = PAPI_NULL;

    /* Initialize the PAPI library */
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT)
        handle_error(1);

    /* Create the EventSet */
    if (PAPI_create_eventset(&EventSet) != PAPI_OK)
        handle_error(1);

    /* Add Total Instructions Executed to our EventSet */
    if (PAPI_add_event(EventSet, PAPI_TOT_INS) != PAPI_OK)
        handle_error(1);

    /* Call handler every 100000 instructions */
    retval = PAPI_overflow(EventSet, PAPI_TOT_INS, WINDOW_SIZE, 0, handler);
    if (retval != PAPI_OK)
        handle_error(1);

    /* Start counting */
    if (PAPI_start(EventSet) != PAPI_OK)
        handle_error(1);

    // start of execution


    int fc_weights[5][5];
    int fc_biasses[5];
    int fc_inputs[5];
    int fc_outputs[5];
    int fc_input_num;
    int fc_output_num;
    int fc_af_type;

    int cnn_weights[5][5][5][5];
    int cnn_biasses[5];
    int cnn_inputs[5][5][5];
    int cnn_outputs[5][5][5];
    int cnn_output_channel;
    int cnn_input_channel;
    int cnn_input_size;
    int cnn_kernel_size;
    int cnn_stride;
    int cnn_zero_padd;
    int cnn_af_type;
    int cnn_pool_type;

	int itr;

    for (itr = 0; itr < TEST_ITR; itr++) {
        //layer 1
        cnn_output_channel = 5;
        cnn_input_channel = 5;
        cnn_input_size = 5;
        cnn_kernel_size = 2;
        cnn_stride = 1;
        cnn_zero_padd = 0;
        cnn_af_type = 2;

        cnn_input_generator(cnn_inputs, cnn_input_channel, cnn_input_size);
        cnn_weight_generator(cnn_weights, cnn_biasses, cnn_input_channel, cnn_output_channel, cnn_kernel_size);
        cnn_layer(cnn_weights, cnn_biasses, cnn_inputs, cnn_outputs, cnn_input_channel, cnn_output_channel, cnn_input_size, cnn_kernel_size, cnn_stride, cnn_zero_padd, cnn_af_type);

        cnn_output_channel = 5;
        cnn_input_channel = 5;
        cnn_input_size = 4;
        cnn_kernel_size = 2;
        cnn_stride = 2;
        cnn_zero_padd = 0;
        cnn_pool_type = 0;

        cnn_pool(cnn_outputs, cnn_inputs, cnn_input_channel, cnn_input_size, cnn_kernel_size, cnn_stride, cnn_zero_padd, cnn_pool_type);

        //layer conversion
        cnn_to_fc(cnn_inputs, 5, 1, fc_inputs);

        //first FC layer
        fc_input_num = 5;
        fc_output_num = 5;
        fc_af_type = 0;

        fc_input_generator(fc_inputs, fc_input_num);
        fc_weight_generator(fc_weights, fc_biasses, fc_input_num, fc_output_num);
        fc_layer(fc_weights, fc_biasses, fc_inputs, fc_outputs, fc_input_num, fc_output_num, fc_af_type);

    }

    printf("total miss: %d\n", total_miss);
	return 0;

}
