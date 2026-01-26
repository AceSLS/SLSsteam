import json
import requests
import sys

API_KEY = "B65FE397A2BDAB057D4554E304B09E8B"

API_GET_ITEM_DEF_META = "https://api.steampowered.com/IInventoryService/GetItemDefMeta/v1"
API_GET_ITEM_DEF_ARCHIVE = "https://api.steampowered.com/IGameInventory/GetItemDefArchive/v1"


def get_item_def_meta(app_id: int) -> str:
    req = requests.request(
        "GET", f"{API_GET_ITEM_DEF_META}/?key={API_KEY}&appid={app_id}")

    if req.status_code != 200:
        print("GetItemDefMeta failed!")
        exit(1)

    parsed = json.loads(req.text)
    return parsed["response"]["digest"]


def get_item_def_archive(app_id: int, digest: str) -> list[str]:
    req = requests.request(
        "GET", f"{API_GET_ITEM_DEF_ARCHIVE}/?appid={app_id}&digest={digest}")

    if req.status_code != 200:
        print("GetItemDefArchive failed!")
        exit(1)

    parsed = json.loads(req.text[:-1])
    return parsed


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Missing appid parameter!\nUsage: python {sys.argv[0]} appId")
        exit(1)

    app_id = sys.argv[1]

    if not app_id.isnumeric():
        print(f"Failed to parse {app_id}")
        exit(1)

    digest = get_item_def_meta(app_id)
    items = get_item_def_archive(app_id, digest)

    for itm in items:
        print(f"{itm["itemdefid"]}: 1 #{itm["name"]} ({itm["description"]})")


main()
