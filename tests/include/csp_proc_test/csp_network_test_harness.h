/**
 * This file contains a test harness for tests requiring a CSP network.
 *
 * The test harness sets up a CSP network with three nodes at addresses 1, 2, and 3, communicating via ZMQ.
 *
 * There are 2 ways, so use the setup. First, ensure this header file is included. Second, define a new test suite that defines the following init and fini:
 * `TestSuite(..., .init = setup_network, .fini = teardown_network);`
 *
 * Or simply tap into the `csp_network` test suite defined at the bottom of this file. I.e. new tests should follow:
 * `Test(csp_network, ...)`
 *
 * The setup also provides a default set of parameters for each node. The parameters are the following (where `n` is the node number):
 * - p_uint8_arr_`n`: uint8_t[32]
 * - p_uint8_`n`: uint8_t
 * - p_uint16_arr_`n`: uint16_t[32]
 * - p_uint16_`n`: uint16_t
 * - p_uint32_arr_`n`: uint32_t[32]
 * - p_uint32_`n`: uint32_t
 * - p_uint64_arr_`n`: uint64_t[32]
 * - p_uint64_`n`: uint64_t
 * - p_int8_arr_`n`: int8_t[32]
 * - p_int8_`n`: int8_t
 * - p_int16_arr_`n`: int16_t[32]
 * - p_int16_`n`: int16_t
 * - p_int32_arr_`n`: int32_t[32]
 * - p_int32_`n`: int32_t
 * - p_int64_arr_`n`: int64_t[32]
 * - p_int64_`n`: int64_t
 * - p_float_arr_`n`: float[32]
 * - p_float_`n`: float
 * - p_double_arr_`n`: double[32]
 * - p_double_`n`: double
 *
 * The vmem_t of node `n` is defined as vmem_config_n if more parameters are needed in the vmem for a given test.
 */

#ifndef CSP_TEST_HARNESS_H
#define CSP_TEST_HARNESS_H

#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include <criterion/criterion.h>

#include <csp/csp.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <param/param.h>
#include <param/param_server.h>
#include <vmem/vmem.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_ram.h>

#include <csp_proc/proc_server.h>
#include <csp_proc/proc_runtime.h>
#include <csp_proc/proc_memory.h>

typedef struct {
	csp_iface_t * iface;
} csp_node_fixture_t;

#define DEFINE_PARAMS(node)                                                                                                                      \
	param_t p_uint8_##node;                                                                                                                      \
	param_t p_uint8_arr_##node;                                                                                                                  \
	param_t p_uint16_##node;                                                                                                                     \
	param_t p_uint16_arr_##node;                                                                                                                 \
	param_t p_uint32_##node;                                                                                                                     \
	param_t p_uint32_arr_##node;                                                                                                                 \
	param_t p_uint64_##node;                                                                                                                     \
	param_t p_uint64_arr_##node;                                                                                                                 \
	param_t p_int8_##node;                                                                                                                       \
	param_t p_int8_arr_##node;                                                                                                                   \
	param_t p_int16_##node;                                                                                                                      \
	param_t p_int16_arr_##node;                                                                                                                  \
	param_t p_int32_##node;                                                                                                                      \
	param_t p_int32_arr_##node;                                                                                                                  \
	param_t p_int64_##node;                                                                                                                      \
	param_t p_int64_arr_##node;                                                                                                                  \
	param_t p_float_##node;                                                                                                                      \
	param_t p_float_arr_##node;                                                                                                                  \
	param_t p_double_##node;                                                                                                                     \
	param_t p_double_arr_##node;                                                                                                                 \
	PARAM_DEFINE_STATIC_VMEM(1, p_uint8_arr_##node, PARAM_TYPE_UINT8, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x00, "uint8 array (32)");      \
	PARAM_DEFINE_STATIC_VMEM(2, p_uint8_##node, PARAM_TYPE_UINT8, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x20, "uint8");                      \
	PARAM_DEFINE_STATIC_VMEM(3, p_uint16_arr_##node, PARAM_TYPE_UINT16, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x21, "uint16 array (32)");   \
	PARAM_DEFINE_STATIC_VMEM(4, p_uint16_##node, PARAM_TYPE_UINT16, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x61, "uint16");                   \
	PARAM_DEFINE_STATIC_VMEM(5, p_uint32_arr_##node, PARAM_TYPE_UINT32, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x63, "uint32 array (32)");   \
	PARAM_DEFINE_STATIC_VMEM(6, p_uint32_##node, PARAM_TYPE_UINT32, 1, 0, PM_CONF, NULL, NULL, config_##node, 0xE3, "uint32");                   \
	PARAM_DEFINE_STATIC_VMEM(7, p_uint64_arr_##node, PARAM_TYPE_UINT64, 32, 1, PM_CONF, NULL, NULL, config_##node, 0xE7, "uint64 array (32)");   \
	PARAM_DEFINE_STATIC_VMEM(8, p_uint64_##node, PARAM_TYPE_UINT64, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x1E7, "uint64");                  \
	PARAM_DEFINE_STATIC_VMEM(9, p_int8_arr_##node, PARAM_TYPE_INT8, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x1EF, "int8 array (32)");        \
	PARAM_DEFINE_STATIC_VMEM(10, p_int8_##node, PARAM_TYPE_INT8, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x20F, "int8");                       \
	PARAM_DEFINE_STATIC_VMEM(11, p_int16_arr_##node, PARAM_TYPE_INT16, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x210, "int16 array (32)");    \
	PARAM_DEFINE_STATIC_VMEM(12, p_int16_##node, PARAM_TYPE_INT16, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x250, "int16");                    \
	PARAM_DEFINE_STATIC_VMEM(13, p_int32_arr_##node, PARAM_TYPE_INT32, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x252, "int32 array (32)");    \
	PARAM_DEFINE_STATIC_VMEM(14, p_int32_##node, PARAM_TYPE_INT32, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x2D2, "int32");                    \
	PARAM_DEFINE_STATIC_VMEM(15, p_int64_arr_##node, PARAM_TYPE_INT64, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x2D6, "int64 array (32)");    \
	PARAM_DEFINE_STATIC_VMEM(16, p_int64_##node, PARAM_TYPE_INT64, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x3D6, "int64");                    \
	PARAM_DEFINE_STATIC_VMEM(17, p_float_arr_##node, PARAM_TYPE_FLOAT, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x3DE, "float array (32)");    \
	PARAM_DEFINE_STATIC_VMEM(18, p_float_##node, PARAM_TYPE_FLOAT, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x45E, "float");                    \
	PARAM_DEFINE_STATIC_VMEM(19, p_double_arr_##node, PARAM_TYPE_DOUBLE, 32, 1, PM_CONF, NULL, NULL, config_##node, 0x462, "double array (32)"); \
	PARAM_DEFINE_STATIC_VMEM(20, p_double_##node, PARAM_TYPE_DOUBLE, 1, 0, PM_CONF, NULL, NULL, config_##node, 0x562, "double");

static void * vmem_server_task(void * param) {
	vmem_server_loop(param);
	return NULL;
}

static void * router_task(void * param) {
	while (1) {
		csp_route_work();
	}
	return NULL;
}

uint32_t _serial0;

void serial_init(void) {
	_serial0 = rand();
}

uint32_t serial_get(void) {
	return _serial0;
}

static csp_node_fixture_t * csp_node_setup(int addr) {
	csp_node_fixture_t * fixture = proc_malloc(sizeof(csp_node_fixture_t));

	srand(time(NULL));
	serial_init();

	char hostname[50];
	sprintf(hostname, "CSP-Node-Fixture-%d", addr);
	csp_conf.hostname = hostname;
	csp_conf.model = "Test";
	csp_conf.revision = "1";
	csp_conf.version = 2;
	csp_conf.dedup = CSP_DEDUP_OFF;

	csp_init();
	csp_zmqhub_init_filter2("ZMQ", "localhost", addr, 8, true, &fixture->iface, NULL, CSP_ZMQPROXY_SUBSCRIBE_PORT, CSP_ZMQPROXY_PUBLISH_PORT);

	fixture->iface->is_default = true;
	fixture->iface->addr = addr;
	fixture->iface->netmask = 8;
	fixture->iface->name = "ZMQ";

	csp_bind_callback(csp_service_handler, CSP_ANY);
	csp_bind_callback(param_serve, PARAM_PORT_SERVER);
	csp_bind_callback(proc_serve, PROC_PORT_SERVER);

	static pthread_t vmem_server_handle;
	pthread_create(&vmem_server_handle, NULL, &vmem_server_task, NULL);

	static pthread_t router_handle;
	pthread_create(&router_handle, NULL, &router_task, NULL);

	return fixture;
}

static void csp_node_teardown(csp_node_fixture_t * fixture) {
	if (fixture != NULL) {
		proc_free(fixture);
	}
}

csp_node_fixture_t * node1_fixture;
csp_node_fixture_t * node2_fixture;
csp_node_fixture_t * node3_fixture;
vmem_t vmem_config_1;
vmem_t vmem_config_2;
vmem_t vmem_config_3;

VMEM_DEFINE_STATIC_RAM(config_1, "config_1", 5000);
DEFINE_PARAMS(1);
VMEM_DEFINE_STATIC_RAM(config_2, "config_2", 5000);
DEFINE_PARAMS(2);
VMEM_DEFINE_STATIC_RAM(config_3, "config_3", 5000);
DEFINE_PARAMS(3);

static void setup_network(void) {
	node1_fixture = csp_node_setup(1);
	node2_fixture = csp_node_setup(2);
	node3_fixture = csp_node_setup(3);
}

static void teardown_network(void) {
	csp_node_teardown(node1_fixture);
	csp_node_teardown(node2_fixture);
	csp_node_teardown(node3_fixture);
}

TestSuite(csp_network, .init = setup_network, .fini = teardown_network);

#endif  // CSP_TEST_HARNESS_H
