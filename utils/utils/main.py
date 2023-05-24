
import json
import logging
from os import getenv
from pathlib import Path
import sys
from typing import Dict, List
import click
from colorama import Back, Style

import requests

from . import getLogger


FIWARE_IOTA_HOST = getenv("FIWARE_IOTA_HOST")
FIWARE_IOTA_NORTH_PORT = getenv("FIWARE_IOTA_NORTH_PORT")

FIWARE_IOTA_RESOURCE = getenv("FIWARE_IOTA_RESOURCE")
FIWARE_IOTA_APIKEY = getenv("FIWARE_IOTA_APIKEY")
FIWARE_IOTA_DEVICE_ID = getenv("FIWARE_IOTA_DEVICE_ID")

FIWARE_IOTA_SERVICE = getenv('FIWARE_IOTA_SERVICE')
FIWARE_IOTA_SERVICEPATH = getenv("FIWARE_IOTA_SERVICEPATH")

HEADERS = {
    'fiware-service': FIWARE_IOTA_SERVICE,
    'fiware-servicepath': FIWARE_IOTA_SERVICEPATH
}

logger = getLogger(__file__, '')


def check_env_vars():
    env_vars = [FIWARE_IOTA_HOST, FIWARE_IOTA_NORTH_PORT,
                FIWARE_IOTA_RESOURCE,
                FIWARE_IOTA_APIKEY, FIWARE_IOTA_DEVICE_ID,
                FIWARE_IOTA_SERVICE, FIWARE_IOTA_SERVICEPATH]

    if any(env_var is None for env_var in env_vars):
        logger.critical("Environment variables not set")
        sys.exit(1)


def check_iot_agent_health():
    try:
        requests.get(
            f"http://{FIWARE_IOTA_HOST}:{FIWARE_IOTA_NORTH_PORT}/iot/about")

    except ConnectionError:
        logger.error("IoT Agent not available, exiting\n")
        sys.exit(2)


def handle_response_error(response: requests.Response):
    if not response.ok:
        logger.error(
            f"Unexpected response from IoT Agent: {response.status_code} -> {response.json()}\n")
        sys.exit(3)


def check_service_group():
    response = requests.get(
        f"http://{FIWARE_IOTA_HOST}:{FIWARE_IOTA_NORTH_PORT}/iot/services",
        headers=HEADERS
    )

    handle_response_error(response)

    payload: Dict[str, object] = response.json()

    if payload.get('count') == 0:
        print(f"{Back.YELLOW} ERROR {Style.RESET_ALL}")
        return False

    services: List[Dict[str, object]] = payload.get("services")

    if not any(service.get('apikey') == FIWARE_IOTA_APIKEY for service in services):
        print(f"{Back.YELLOW} ERROR {Style.RESET_ALL}")
        return False

    print(f"{Back.GREEN}   OK   {Style.RESET_ALL}")
    return True


def register_service_group():
    logger.info("Registering service group...  ")
    cwd = Path(__file__).parent

    with open(cwd / 'json/register_service_group.json', 'r', encoding='utf8') as f_open:
        payload = json.load(f_open)

    response = requests.post(
        f"http://{FIWARE_IOTA_HOST}:{FIWARE_IOTA_NORTH_PORT}/iot/services",
        headers=HEADERS,
        json=payload
    )

    if response.ok:
        print(f"{Back.GREEN}   OK   {Style.RESET_ALL}")

    else:
        print(f"{Back.YELLOW} ERROR {Style.RESET_ALL}")
        logger.error(f"{response.status_code} -> {response.json()}")
        sys.exit(4)


def check_device_exists():
    logger.info("Checking IoT Device...   ")

    response = requests.get(
        f"http://{FIWARE_IOTA_HOST}:{FIWARE_IOTA_NORTH_PORT}/iot/devices",
        headers=HEADERS
    )

    handle_response_error(response)

    payload: Dict[str, object] = response.json()

    if payload.get('count') == 0:
        print(f"{Back.YELLOW} ERROR {Style.RESET_ALL}")
        return False

    devices: List[Dict[str, object]] = payload.get('devices')

    if not any(device.get('device_id') == FIWARE_IOTA_DEVICE_ID for device in devices):
        print(f"{Back.YELLOW} ERROR {Style.RESET_ALL}")
        return False

    print(f"{Back.GREEN}   OK   {Style.RESET_ALL}")
    return True


def register_device():
    logger.info("Registering IoT Device...   ")

    cwd = Path(__file__).parent

    with open(cwd / 'json/create_iot_device.json', 'r', encoding='utf8') as f_open:
        payload = json.load(f_open)

    response = requests.post(
        f"http://{FIWARE_IOTA_HOST}:{FIWARE_IOTA_NORTH_PORT}/iot/devices",
        headers=HEADERS,
        json=payload
    )

    if response.ok:
        print(f"{Back.GREEN}   OK   {Style.RESET_ALL}")

    else:
        print(f"{Back.YELLOW} ERROR {Style.RESET_ALL}")
        logger.error(f"{response.status_code} -> {response.json()}")
        sys.exit(4)


def check_config():
    logger.info('Initializing...   ')
    check_env_vars()
    check_iot_agent_health()
    print(f"{Back.GREEN}  DONE  {Style.RESET_ALL}")

    logger.info("Checking service group...   ")

    if not check_service_group():
        # register it
        register_service_group()

    if not check_device_exists():
        # register it
        register_device()


def delete_service_group():
    logger.info("Deleting IoT service group...   ")

    response = requests.delete(
        f"http://{FIWARE_IOTA_HOST}:{FIWARE_IOTA_NORTH_PORT}/iot/services",
        params={
            'resource': FIWARE_IOTA_RESOURCE,
            'apikey': FIWARE_IOTA_APIKEY
        },
        headers=HEADERS
    )

    if response.ok:
        print(f"{Back.GREEN}   OK   {Style.RESET_ALL}")

    else:
        print(f"{Back.YELLOW} ERROR {Style.RESET_ALL}")
        logger.error(f"{response.status_code} -> {response.json()}")

        sys.exit(4)


def delete_iot_device():
    logger.info("Deleting IoT device...   ")

    response = requests.delete(
        f"http://{FIWARE_IOTA_HOST}:{FIWARE_IOTA_NORTH_PORT}/iot/devices/{FIWARE_IOTA_DEVICE_ID}",
        headers=HEADERS
    )

    if response.ok:
        print(f"{Back.GREEN}   OK   {Style.RESET_ALL}")

    else:
        print(f"{Back.YELLOW} ERROR {Style.RESET_ALL}")
        logger.error(f"{response.status_code} -> {response.json()}")

        sys.exit(4)


@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    if ctx.invoked_subcommand is None:
        check_config()


@cli.command()
def clear():
    logger.info("Clearing IoT Agent...   ")
    print(f"{Back.BLUE} RUNNING {Style.RESET_ALL}")

    delete_service_group()
    delete_iot_device()


if __name__ == '__main__':
    check_config()
