import logging
from logging import LogRecord
import logging.config

from colorama import Fore, Style
from dotenv import load_dotenv


load_dotenv()


def filter_maker(level):
    level = getattr(logging, level)

    def filter(record):
        return record.levelno == level

    return filter


class DebugFilter(logging.Filter):
    def filter(self, record: LogRecord) -> bool:
        return record.levelno == logging.DEBUG


class InfoFilter(logging.Filter):
    def filter(self, record: LogRecord) -> bool:
        return record.levelno == logging.INFO


class WarningFilter(logging.Filter):
    def filter(self, record: LogRecord) -> bool:
        return record.levelno == logging.WARNING


class ErrorFilter(logging.Filter):
    def filter(self, record: LogRecord) -> bool:
        return record.levelno == logging.ERROR


class CriticalFilter(logging.Filter):
    def filter(self, record: LogRecord) -> bool:
        return record.levelno == logging.CRITICAL


def getLogger(name, terminator: str = '\n'):

    # formats
    debug_format = f"[{Fore.LIGHTBLACK_EX}%(levelname)-8s{Fore.RESET}]: %(message)s"
    info_format = f"[{Fore.GREEN}%(levelname)-8s{Fore.RESET}]: %(message)s"
    warning_format = f"[{Fore.YELLOW}%(levelname)-8s{Fore.RESET}]: %(message)s"
    error_format = f"[{Fore.RED}%(levelname)-8s{Fore.RESET}]: %(message)s"
    critical_format = f"[{Style.BRIGHT}{Fore.RED}%(levelname)-8s{Style.RESET_ALL}]: %(message)s"

    formats = [debug_format,
               info_format,
               warning_format,
               error_format,
               critical_format
               ]
    filters = [DebugFilter(),
               InfoFilter(),
               WarningFilter(),
               ErrorFilter(),
               CriticalFilter()
               ]

    logger = logging.getLogger(name)

    for fmt, fltr in zip(formats, filters):
        handler = logging.StreamHandler()
        handler.terminator = terminator

        handler.setFormatter(logging.Formatter(fmt))
        handler.addFilter(fltr)

        logger.addHandler(handler)

    logger.setLevel(logging.INFO)

    return logger
