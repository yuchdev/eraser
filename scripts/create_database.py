import os
import sys
import shutil
import sqlite3
import logging
import argparse
import hashlib

sys.path.append('../../tools/py_utils')
import log_helper
logger = log_helper.setup_logger(name="create_database", level=logging.DEBUG, log_to_file=False)


def test_select(cur):
    """
    :param cur: Valid database connection cursor
    :return: size of table
    """
    cur.execute("SELECT * FROM filetable")
    test_list = cur.fetchall()
    return len(test_list)


# noinspection PyBroadException
def main():
    """
    :return: return code
    """
    parser = argparse.ArgumentParser(description='Command-line interface')
    parser.add_argument('--db-name',
                        help='Generated database name',
                        dest='db_name')

    parser.add_argument('--output-dir',
                        help='Directory where to put database',
                        dest='output_dir',
                        default=".",
                        required=False)

    args = parser.parse_args()
    try:
        if os.path.isfile(args.db_name):
            logger.info("Previous database present, delete file")
            os.remove(args.db_name)
        db_connection = sqlite3.connect(args.db_name)
        logger.info("Connected to database")

        cur = db_connection.cursor()
        cur.execute(
            "CREATE TABLE IF NOT EXISTS filetable("
            "hash TEXT PRIMARY KEY,"
            "filename TEXT NOT NULL,"
            "entropy REAL NOT NULL,"
            "flags INT8 NOT NULL)")
        logger.info("Created table")

        # create hash which is key
        file_name = 'C:/Temp/my.dll'
        hasher = hashlib.md5()
        b = bytearray(file_name, encoding="utf-8")
        hasher.update(b)
        myhash = hasher.hexdigest()
        logger.info("Hash: {0}".format(myhash))

        if myhash is None:
            return 0

        cur.execute("INSERT INTO filetable(hash, filename, entropy, flags) VALUES('{0}','{1}', {2}, {3})".format(
            myhash,
            file_name,
            6.14,
            0))

        db_connection.commit()
        list_size = test_select(cur)
        logger.info("Checked table creation")

        if list_size == 1:
            logger.info("INSERT tested")

        cur.execute("DELETE FROM filetable WHERE hash='{0}'".format(myhash))
        db_connection.commit()
        list_size = test_select(cur)
        if list_size == 0:
            logger.info("DELETE tested")

        if args.output_dir != ".":
            shutil.copy(args.db_name, os.path.join(args.output_dir, args.db_name))
            logger.info("Database file copied to {0}".format(args.output_dir))
    except Exception as e:
        logger.error("Error while creating database: {0}".format(e))
        return 3
    return 0


###########################################################################
if __name__ == '__main__':
    sys.exit(main())
