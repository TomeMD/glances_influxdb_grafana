/*
MIT License

Copyright (c) 2014-2023 Universidade da Coruña

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "papi.h"
#include "influxdb-client/ic.h"

#define MAX_EVENTS 32

int main (int argc, char **argv) {
    int retval,cid,rapl_cid=-1,numcmp;
    int i,code,enum_retval,seconds_interval,microseconds_interval,max_time;
    int num_events = 0;
    int EventSet = PAPI_NULL;
    long long values[MAX_EVENTS];
    char event_name[BUFSIZ];
    PAPI_event_info_t evinfo;
    const PAPI_component_info_t *cmpinfo = NULL;
    long long start_time,before_time,after_time,offset_time;
    double elapsed_time,total_time;
    size_t array_length;
    char events[MAX_EVENTS][BUFSIZ];
    char units[MAX_EVENTS][BUFSIZ];
    char* arg_filename;
    FILE *fff_energy_package, *fff_energy_dram, *fff_energy_pp0, *fff_energy_pp1, *fff_energy_uncore_package, *fff_energy_psys;
    FILE *fff_power_package, *fff_power_dram, *fff_power_pp0, *fff_power_pp1, *fff_power_uncore_package, *fff_power_psys;

    seconds_interval = 1;
    max_time = 0;

    if (argc == 1) {
    char *prefix = "out";
    arg_filename = (char*)malloc(strlen(prefix)+1);
    sprintf(arg_filename, "%s", prefix);
    } else if (argc == 2) {
        arg_filename = argv[1];
    } else if (argc == 3) {
        arg_filename = argv[1];
    sscanf(argv[2], "%i", &seconds_interval);
    } else if (argc == 4) {
        arg_filename = argv[1];
    sscanf(argv[2], "%i", &seconds_interval);
        sscanf(argv[3], "%i", &max_time);
    } else {
    fprintf(stderr, "Usage: %s OUTPUT_PREFIX INTERVAL_SECONDS [MAX_TIME_SECONDS]\n", argv[0]);
    exit(-1);
    }

    microseconds_interval = seconds_interval * 1e6;
    array_length = strlen(arg_filename)+50;
    char output_filename_energy_package[array_length],
    output_filename_energy_dram[array_length],
    output_filename_energy_pp0[array_length],
    output_filename_energy_pp1[array_length],
    output_filename_energy_uncore_package[array_length],
    output_filename_energy_psys[array_length],
    output_filename_power_package[array_length],
    output_filename_power_dram[array_length],
    output_filename_power_pp0[array_length],
    output_filename_power_pp1[array_length],
    output_filename_power_uncore_package[array_length],
    output_filename_power_psys[array_length];

    sprintf(output_filename_energy_package, "%s_energy_joules_package.csv", arg_filename);
    sprintf(output_filename_energy_dram, "%s_energy_joules_dram.csv", arg_filename);
    sprintf(output_filename_energy_pp0, "%s_energy_joules_pp0.csv", arg_filename);
    sprintf(output_filename_energy_pp1, "%s_energy_joules_pp1.csv", arg_filename);
    sprintf(output_filename_energy_uncore_package, "%s_energy_joules_uncore_package.csv", arg_filename);
    sprintf(output_filename_energy_psys, "%s_energy_joules_psys.csv", arg_filename);
    sprintf(output_filename_power_package, "%s_power_package.csv", arg_filename);
    sprintf(output_filename_power_dram, "%s_power_dram.csv", arg_filename);
    sprintf(output_filename_power_pp0, "%s_power_pp0.csv", arg_filename);
    sprintf(output_filename_power_pp1, "%s_power_pp1.csv", arg_filename);
    sprintf(output_filename_power_uncore_package, "%s_power_uncore_package.csv", arg_filename);
    sprintf(output_filename_power_psys, "%s_power_psys.csv", arg_filename);

    printf("Output prefix: %s\n", arg_filename);
    printf("Interval: %i s (%i us)\n", seconds_interval, microseconds_interval);
    printf("Max time: %i s\n", max_time);

    /* PAPI Initialization */
    retval = PAPI_library_init( PAPI_VER_CURRENT );
    if ( retval != PAPI_VER_CURRENT ) {
	fprintf(stderr, "PAPI_library_init failed\n");
	exit(-1);
    }

    numcmp = PAPI_num_components();

    for (cid=0; cid<numcmp; cid++) {
		if ((cmpinfo = PAPI_get_component_info(cid)) == NULL) {
			fprintf(stderr,"PAPI_get_component_info failed\n");
			exit(1);
		}

		if (strstr(cmpinfo->name, "rapl")) {
			rapl_cid=cid;
			printf("Found RAPL component at cid %d\n", rapl_cid);

			if (cmpinfo->disabled) {
				fprintf(stderr, "RAPL component disabled: %s\n", cmpinfo->disabled_reason);
				exit(-1);
			}
			break;
		}
    }

    /* Component not found */
    if (cid == numcmp) {
 	fprintf(stderr, "Error! No RAPL component found!\n");
	exit(1);
    }

    /* Find Events */
    code = PAPI_NATIVE_MASK;
    enum_retval = PAPI_enum_cmp_event(&code, PAPI_ENUM_FIRST, cid);

    while (enum_retval == PAPI_OK) {
	retval = PAPI_event_code_to_name(code, event_name);
	if (retval != PAPI_OK) {
		fprintf(stderr, "Error translating %#x\n", code);
		exit(-1);
	}

	printf("Found event: %s\n", event_name);

	if (strstr(event_name, "ENERGY") != NULL && strstr(event_name, "ENERGY_CNT") == NULL) {
		strncpy(events[num_events], event_name, BUFSIZ);

	 	/* Find additional event information: unit, data type */
	        retval = PAPI_get_event_info(code, &evinfo);
            	if (retval != PAPI_OK) {
                	fprintf(stderr, "Error getting event info for %#x\n", code);
	                exit(-1);
        	}

            	strncpy(units[num_events],evinfo.units,sizeof(units[0])-1);
            	/* buffer must be null terminated to safely use strstr operation on it below */
            	units[num_events][sizeof(units[0])-1] = '\0';

            	num_events++;

            	if (num_events == MAX_EVENTS) {
                	fprintf(stderr, "Too many events! %d\n", num_events);
	                exit(-1);
            	}
        }

	enum_retval = PAPI_enum_cmp_event(&code, PAPI_ENUM_EVENTS, cid);
    }

    if (num_events == 0) {
    	fprintf(stderr, "Error! No RAPL events found!\n");
	exit(-1);
    }

    /* Create EventSet */
    retval = PAPI_create_eventset(&EventSet);
    if (retval != PAPI_OK) {
	fprintf(stderr, "Error creating EventSet\n");
        exit(-1);
    }

    for (i=0; i<num_events; i++) {
	printf("Saved event: %s\n", events[i]);
	retval = PAPI_add_named_event(EventSet, events[i]);
	if (retval != PAPI_OK) {
		fprintf(stderr, "Error adding event %s\n", events[i]);
        	exit(-1);
	}
    }

    /* Open output files */
    fff_energy_package=fopen(output_filename_energy_package, "w");
    fff_energy_dram=fopen(output_filename_energy_dram, "w");
    fff_energy_pp0=fopen(output_filename_energy_pp0, "w");
    fff_energy_pp1=fopen(output_filename_energy_pp1, "w");
    fff_energy_uncore_package=fopen(output_filename_energy_uncore_package, "w");
    fff_energy_psys=fopen(output_filename_energy_psys, "w");
    fff_power_package=fopen(output_filename_power_package, "w");
    fff_power_dram=fopen(output_filename_power_dram, "w");
    fff_power_pp0=fopen(output_filename_power_pp0, "w");
    fff_power_pp1=fopen(output_filename_power_pp1, "w");
    fff_power_uncore_package=fopen(output_filename_power_uncore_package, "w");
    fff_power_psys=fopen(output_filename_power_psys, "w");

    // Create InfluxDB Client
    ic_influx_database("localhost", 8086, "glances", "MyOrg", "MyToken");
    ic_tags("host=rapl");

    if (fff_energy_package == NULL) {
	fprintf(stderr, "Could not open fff_energy_package %s\n", output_filename_energy_package);
	exit(-1);
    }
    if (fff_energy_dram == NULL) {
	fprintf(stderr, "Could not open fff_energy_dram %s\n", output_filename_energy_dram);
	exit(-1);
    }
    if (fff_energy_pp0 == NULL) {
	fprintf(stderr, "Could not open fff_energy_pp0 %s\n", output_filename_energy_pp0);
	exit(-1);
    }
    if (fff_energy_pp1 == NULL) {
	fprintf(stderr, "Could not open fff_energy_pp1 %s\n", output_filename_energy_pp1);
	exit(-1);
    }
    if (fff_energy_uncore_package == NULL) {
	fprintf(stderr, "Could not open fff_energy_uncore_package %s\n", output_filename_energy_uncore_package);
	exit(-1);
    }
    if (fff_energy_psys == NULL) {
	fprintf(stderr, "Could not open fff_energy_psys %s\n", output_filename_energy_psys);
	exit(-1);
    }
    if (fff_power_package == NULL) {
	fprintf(stderr, "Could not open fff_power_package %s\n", output_filename_power_package);
	exit(-1);
    }
    if (fff_power_dram == NULL) {
	fprintf(stderr, "Could not open fff_power_dram %s\n", output_filename_power_dram);
	exit(-1);
    }
    if (fff_power_pp0 == NULL) {
	fprintf(stderr, "Could not open fff_power_pp0 %s\n", output_filename_power_pp0);
	exit(-1);
    }
    if (fff_power_pp1 == NULL) {
	fprintf(stderr, "Could not open fff_power_pp1 %s\n", output_filename_power_pp1);
	exit(-1);
    }
    if (fff_power_uncore_package == NULL) {
	fprintf(stderr, "Could not open fff_power_uncore_package %s\n", output_filename_power_uncore_package);
	exit(-1);
    }
    if (fff_power_psys == NULL) {
	fprintf(stderr, "Could not open fff_power_psys %s\n", output_filename_power_psys);
	exit(-1);
    }

    /* Write file headers */
    fprintf(fff_energy_package, "TIME(s)");
    fprintf(fff_energy_dram, "TIME(s)");
    fprintf(fff_energy_pp0, "TIME(s)");
    fprintf(fff_energy_pp1, "TIME(s)");
    fprintf(fff_energy_uncore_package, "TIME(s)");
    fprintf(fff_energy_psys, "TIME(s)");
    fprintf(fff_power_package, "TIME(s)");
    fprintf(fff_power_dram, "TIME(s)");
    fprintf(fff_power_pp0, "TIME(s)");
    fprintf(fff_power_pp1, "TIME(s)");
    fprintf(fff_power_uncore_package, "TIME(s)");
    fprintf(fff_power_psys, "TIME(s)");

    for (i=0; i<num_events; i++) {

        if (strstr(events[i], "DRAM_")) {
            fprintf(fff_energy_dram,", %s(J)", events[i]);
            fprintf(fff_power_dram,", %s(W)", events[i]);
        } else if (strstr(events[i], "PP0_")) {
            fprintf(fff_energy_pp0,", %s(J)", events[i]);
            fprintf(fff_power_pp0,", %s(W)", events[i]);
        } else if (strstr(events[i], "PP1_")) {
            fprintf(fff_energy_pp1,", %s(J)", events[i]);
            fprintf(fff_power_pp1,", %s(W)", events[i]);
        } else if (strstr(events[i], "PSYS_")) {
            fprintf(fff_energy_psys,", %s(J)", events[i]);
            fprintf(fff_power_psys,", %s(W)", events[i]);
        } else if (strstr(events[i], "PACKAGE_")) {
            fprintf(fff_energy_package,", %s(J)", events[i]);
            fprintf(fff_power_package,", %s(W)", events[i]);
            if (strstr(events[i], "PACKAGE0")) {
                fprintf(fff_energy_uncore_package,", UNCORE_ENERGY:PACKAGE0(J)");
                fprintf(fff_power_uncore_package,", UNCORE_POWER:PACKAGE0(W)");
            }
            if (strstr(events[i], "PACKAGE1")) {
                fprintf(fff_energy_uncore_package,", UNCORE_ENERGY:PACKAGE1(J)");
                fprintf(fff_power_uncore_package,", UNCORE_POWER:PACKAGE1(W)");
            }
        } else {
            fprintf(stderr, "Error! Unexpected event %s found!\n", events[i]);
        }
    }

    fprintf(fff_energy_package, "\n");
    fprintf(fff_energy_dram, "\n");
    fprintf(fff_energy_pp0, "\n");
    fprintf(fff_energy_pp1, "\n");
    fprintf(fff_energy_uncore_package, "\n");
    fprintf(fff_energy_psys, "\n");
    fprintf(fff_power_package, "\n");
    fprintf(fff_power_dram, "\n");
    fprintf(fff_power_pp0, "\n");
    fprintf(fff_power_pp1, "\n");
    fprintf(fff_power_uncore_package, "\n");
    fprintf(fff_power_psys, "\n");

    fflush(fff_energy_package);
    fflush(fff_energy_dram);
    fflush(fff_energy_pp0);
    fflush(fff_energy_pp1);
    fflush(fff_energy_uncore_package);
    fflush(fff_energy_psys);
    fflush(fff_power_package);
    fflush(fff_power_dram);
    fflush(fff_power_pp0);
    fflush(fff_power_pp1);
    fflush(fff_power_uncore_package);
    fflush(fff_power_psys);

    printf("Starting measuring loop...\n");
    fflush(stdout);
    fflush(stderr);

    double energy, power;
    double energy_pp0_pkg0 = 0, power_pp0_pkg0 = 0, energy_pp0_pkg1 = 0, power_pp0_pkg1 = 0;
    double energy_pkg0 = 0, power_pkg0 = 0, energy_pkg1 = 0, power_pkg1 = 0;
    char column_joules[40], column_watts[40], measure_energy[40], measure_power[40];
    start_time=PAPI_get_real_nsec();
    after_time=start_time;

    /* Main loop */
    while (1) {
	/* Start counting */
	before_time=PAPI_get_real_nsec();
	retval = PAPI_start(EventSet);
	if (retval != PAPI_OK) {
		fprintf(stderr, "PAPI_start() failed\n");
		exit(-1);
	}

	offset_time=(PAPI_get_real_nsec() - after_time)/1000 + 50;
	usleep(microseconds_interval - offset_time);

	/* Stop counting */
	after_time=PAPI_get_real_nsec();
	retval = PAPI_stop(EventSet, values);
	if (retval != PAPI_OK) {
		fprintf(stderr, "PAPI_stop() failed\n");
           		exit(-1);
	}

	total_time=((double)(after_time-start_time))/1.0e9;
	elapsed_time=((double)(after_time-before_time))/1.0e9;

	fprintf(fff_energy_package, "%.4f", total_time);
	fprintf(fff_energy_dram, "%.4f", total_time);
	fprintf(fff_energy_pp0, "%.4f", total_time);
	fprintf(fff_energy_pp1, "%.4f", total_time);
       	fprintf(fff_energy_uncore_package, "%.4f", total_time);
        fprintf(fff_energy_psys, "%.4f", total_time);
	fprintf(fff_power_package, "%.4f", total_time);
	fprintf(fff_power_dram, "%.4f", total_time);
	fprintf(fff_power_pp0, "%.4f", total_time);
	fprintf(fff_power_pp1, "%.4f", total_time);
       	fprintf(fff_power_uncore_package, "%.4f", total_time);
        fprintf(fff_power_psys, "%.4f", total_time);

        energy_pkg0 = 0;
        energy_pkg1 = 0;
	energy_pp0_pkg0 = 0;
        energy_pp0_pkg1 = 0;

        for (i=0; i<num_events; i++) {
            /* Energy consumption is returned in nano-Joules (nJ) */

            energy = ((double)values[i] / 1.0e9);
            power = energy / elapsed_time;

            strcpy(column_joules, events[i]);
            strcat(column_joules, "(J)");
            strcpy(column_watts, events[i]);
            strcat(column_watts, "(W)");

            printf("events[%i]=%s, values[%i]=%lli\n", i, events[i], i, values[i]);
            printf("Energy %.3f, Power %.3f\n", energy, power);

	    if (energy == 0)
		continue;

            if (strstr(events[i], "DRAM_")) {
                fprintf(fff_energy_dram, ", %.3f", energy);
                fprintf(fff_power_dram, ", %.3f", power);
                strcpy(measure_energy, "ENERGY_DRAM");
                strcpy(measure_power, "POWER_DRAM");
            } else if (strstr(events[i], "PP0_")) {
		fprintf(fff_energy_pp0, ", %.3f", energy);
		fprintf(fff_power_pp0, ", %.3f", power);
                strcpy(measure_energy, "ENERGY_PP0");
                strcpy(measure_power, "POWER_PP0");
                 if (strstr(events[i], "PACKAGE0")) {
                         energy_pp0_pkg0 = energy;
                         power_pp0_pkg0 = power;
                 } else if (strstr(events[i], "PACKAGE1")) {
        	         energy_pp0_pkg1 = energy;
	                 power_pp0_pkg1 = power;
                 }
            } else if (strstr(events[i], "PP1_")) {
		fprintf(fff_energy_pp1, ", %.3f", energy);
		fprintf(fff_power_pp1, ", %.3f", power);
                strcpy(measure_energy, "ENERGY_PP1");
                strcpy(measure_power, "POWER_PP1");
	    } else if (strstr(events[i], "PSYS_")) {
		fprintf(fff_energy_psys, ", %.3f", energy);
		fprintf(fff_power_psys, ", %.3f", power);
                strcpy(measure_energy, "ENERGY_PSYS");
                strcpy(measure_power, "POWER_PSYS");
            } else if (strstr(events[i], "PACKAGE_")) {
		fprintf(fff_energy_package, ", %.3f", energy);
		fprintf(fff_power_package, ", %.3f", power);
                strcpy(measure_energy, "ENERGY_PACKAGE");
                strcpy(measure_power, "POWER_PACKAGE");
                if (strstr(events[i], "PACKAGE0")) {
                        energy_pkg0 = energy;
                        power_pkg0 = power;
                } else if (strstr(events[i], "PACKAGE1")) {
                        energy_pkg1 = energy;
                	power_pkg1 = power;
                }
            } else {
                fprintf(stderr, "Error! Unexpected event %s found!\n", events[i]);
	    }

            ic_measure(measure_energy);
                ic_double("TIME(s)", total_time);
                ic_double(column_joules, energy);
            ic_measureend();
            ic_measure(measure_power);
                ic_double("TIME(s)", total_time);
                ic_double(column_watts, power);
            ic_measureend();
            ic_push(); // Send metrics to InfluxDB
	}

	if (energy_pp0_pkg0 != 0) {
	    //printf("energy_pkg0 %.3f, energy_pp0_pkg0 %.3f\n", energy_pkg0, energy_pp0_pkg0);
            fprintf(fff_energy_uncore_package, ", %.3f", energy_pkg0 - energy_pp0_pkg0);
            fprintf(fff_power_uncore_package, ", %.3f", power_pkg0 - power_pp0_pkg0);
            ic_measure("UNCORE_ENERGY_PACKAGE");
                ic_double("TIME(s)", total_time);
                ic_double("UNCORE_ENERGY:PACKAGE0(J)", energy_pkg0 - energy_pp0_pkg0);
            ic_measureend();

            ic_measure("UNCORE_POWER_PACKAGE");
                ic_double("TIME(s)", total_time);
                ic_double("UNCORE_POWER:PACKAGE0(J)", power_pkg0 - power_pp0_pkg0);
            ic_measureend();

            ic_push();
        }

        if (energy_pp0_pkg1 != 0) {
	    //printf("energy_pkg1 %.3f, energy_pp0_pkg1 %.3f\n", energy_pkg1, energy_pp0_pkg1);
            fprintf(fff_energy_uncore_package, ", %.3f", energy_pkg1 - energy_pp0_pkg1);
            fprintf(fff_power_uncore_package, ", %.3f", power_pkg1 - power_pp0_pkg1);

            ic_measure("UNCORE_ENERGY_PACKAGE");
                ic_double("TIME(s)", total_time);
                ic_double("UNCORE_ENERGY:PACKAGE1(J)", energy_pkg1 - energy_pp0_pkg1);
            ic_measureend();

            ic_measure("UNCORE_POWER_PACKAGE");
                ic_double("TIME(s)", total_time);
                ic_double("UNCORE_POWER:PACKAGE1(J)", power_pkg1 - power_pp0_pkg1);
            ic_measureend();

            ic_push();
        }

        fprintf(fff_energy_package, "\n");
        fprintf(fff_energy_dram, "\n");
        fprintf(fff_energy_pp0, "\n");
        fprintf(fff_energy_pp1, "\n");
        fprintf(fff_energy_uncore_package, "\n");
        fprintf(fff_energy_psys, "\n");
        fprintf(fff_power_package, "\n");
        fprintf(fff_power_dram, "\n");
        fprintf(fff_power_pp0, "\n");
        fprintf(fff_power_pp1, "\n");
        fprintf(fff_power_uncore_package, "\n");
        fprintf(fff_power_psys, "\n");

        fflush(fff_energy_package);
        fflush(fff_energy_dram);
        fflush(fff_energy_pp0);
        fflush(fff_energy_pp1);
        fflush(fff_energy_uncore_package);
        fflush(fff_energy_psys);
        fflush(fff_power_package);
        fflush(fff_power_dram);
        fflush(fff_power_pp0);
        fflush(fff_power_pp1);
        fflush(fff_power_uncore_package);
        fflush(fff_power_psys);

        if (max_time > 0 && total_time >= max_time)
            break;
    }

    printf("Finished loop. Total running time: %.4f s\n", total_time);

    fclose(fff_energy_package);
    fclose(fff_energy_dram);
    fclose(fff_energy_pp0);
    fclose(fff_energy_pp1);
    fclose(fff_energy_uncore_package);
    fclose(fff_energy_psys);
    fclose(fff_power_package);
    fclose(fff_power_dram);
    fclose(fff_power_pp0);
    fclose(fff_power_pp1);
    fclose(fff_power_uncore_package);
    fclose(fff_power_psys);
    
    exit(0);
}