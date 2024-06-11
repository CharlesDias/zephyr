/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define ZEPHYR_ADDR		CONFIG_NET_CONFIG_MY_IPV4_ADDR

#if CONFIG_NET_DEFINED_BROKER_IPV4_ADDR
#define SERVER_ADDR		CONFIG_NET_MQTT_BROKER_IPV4_ADDR
#else
#define SERVER_ADDR		CONFIG_NET_CONFIG_PEER_IPV4_ADDR
#endif

#define SERVER_PORT		1883

#define APP_CONNECT_TIMEOUT_MS	2000
#define APP_SLEEP_MSECS		500

#define APP_CONNECT_TRIES	10

#define APP_MQTT_BUFFER_SIZE	128

#define MQTT_CLIENTID		"zephyr_publisher"

#endif
