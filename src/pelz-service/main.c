/*
 * Contains the main function used to launch the Pelz Key Service
 */

#include <getopt.h>
#include <stdlib.h>

#include "common_table.h"
#include "pelz_service.h"
#include "pelz_log.h"

#include "sgx_urts.h"
#include "pelz_enclave.h"
#include "pelz_enclave_u.h"
sgx_enclave_id_t eid = 0;

#define ENCLAVE_PATH "sgx/pelz_enclave.signed.so"

static void usage(const char *prog)
{
  fprintf(stdout,
    "usage: %s [options] \n\n"
    "options are: \n\n"
    " -h or --help          Help (displays this usage).\n"
    " -m or --max_requests  Maximum number of sockets pelz can make available at any given time, default: 100\n"
    " -v or --verbose       Enable detailed logging.\n", prog);
}

const struct option longopts[] = {
  {"help", no_argument, 0, 'h'},
  {"max_requests", required_argument, 0, 'm'},
  {"verbose", no_argument, 0, 'v'},
  {0, 0, 0, 0}
};

//Main function for the Pelz Service application
int main(int argc, char **argv)
{
  set_app_name("pelz");
  set_app_version("0.0.0");
  set_applog_max_msg_len(1024);
  set_applog_path("/var/log/pelz.log");
  set_applog_severity_threshold(LOG_WARNING);

  int max_requests = 100;
  int options;
  int option_index;
  long mr = 0;

  while ((options = getopt_long(argc, argv, "m:hv", longopts, &option_index)) != -1)
  {
    switch (options)
    {
    case 'h':
      usage(argv[0]);
      return 0;
    case 'm':
      if (optarg && (mr = atol(optarg)) > 0)
      {
        max_requests = (int) mr;
        break;
      }
      else
      {
        pelz_log(LOG_ERR, "max_request must be an integer. Received invalid option '%s'", optarg);
        return 1;
      }
    case 'v':
      set_applog_severity_threshold(LOG_DEBUG);
      set_applog_output_mode(0);
      break;
    default:
      return 1;
    }
  }

  if (optind < argc)
  {
    pelz_log(LOG_ERR, "Invalid arguments found.");
    for (; optind < argc; optind++)
    {
      pelz_log(LOG_ERR, "...Invalid argument: %s", argv[optind]);
    }
    usage(argv[0]);
    return (1);
  }

  TableResponseStatus status;
  int ret;

  sgx_create_enclave(ENCLAVE_PATH, 0, NULL, NULL, &eid, NULL);
  kmyth_unsealed_data_table_initialize(eid, &ret);
  if (ret)
  {
    pelz_log(LOG_ERR, "Unseal Table Init Failure");
    sgx_destroy_enclave(eid);
    return (1);
  }

  private_pkey_init(eid, &status);
  if (status != OK)
  {
    pelz_log(LOG_ERR, "PKEY Init Failure");
    kmyth_unsealed_data_table_cleanup(eid, &ret);
    sgx_destroy_enclave(eid);
    return (1);
  }

  pelz_service((const int) max_requests);

  pelz_log(LOG_INFO, "Shutdown Clean-up Start");
  private_pkey_free(eid, &status);
  if (status != OK)
  {
    pelz_log(LOG_ERR, "PKEY Free Failure");
  }
  pelz_log(LOG_INFO, "Private Pkey Freed");
  kmyth_unsealed_data_table_cleanup(eid, &ret);
  pelz_log(LOG_INFO, "Kmyth Unsealed Data Table Cleanup Complete");
  table_destroy(eid, &status, SERVER);
  pelz_log(LOG_INFO, "Server Table Destroy Complete");
  table_destroy(eid, &status, KEY);
  pelz_log(LOG_INFO, "Key Table Destroy Complete");
  sgx_destroy_enclave(eid);
  pelz_log(LOG_INFO, "SGX Enclave Destroyed");
  return (0);
}
