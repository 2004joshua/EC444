import pymongo
from bson.json_util import dumps

def connect_to_db(db_path):
    """
    Connect to the TingoDB database.
    :param db_path: Path to the TingoDB directory.
    :return: Database connection.
    """
    client = pymongo.MongoClient(f"tingodb://{db_path}")
    db = client["data"]
    return db

def get_data(collection, filter_query=None):
    """
    Fetch data from the TingoDB collection.
    :param collection: TingoDB collection.
    :param filter_query: Query to filter data.
    :return: List of records.
    """
    if filter_query is None:
        filter_query = {}
    return list(collection.find(filter_query))
