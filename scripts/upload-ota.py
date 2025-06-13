import argparse
import requests

def get_version(host:str):
    response = requests.get(f"http://{host}/api/ota/commd/version")
    response = response.json()

    print(f"> Current version: {response['version']}")
    print(f"> Current hash: {response['hash']}")


def upload_ota(zip_path:str, host:str):
    with open(zip_path, "rb") as f:
        files = {'file': ('upload.zip', f, 'application/x-zip-compressed')}
        print("> Upload OTA")
        response = requests.post(f"http://{host}/api/ota/commd/upload", files=files)

        print("Status code:", response.status_code)
        response = response.json()
        print("Response:", response['detail'])

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Upload a OTA file")

    parser.add_argument("ota_file", help="Path to the OTA file to upload")
    parser.add_argument("--host", default="192.168.10.1:8682", help="OTA Server")

    args = parser.parse_args()

    get_version(args.host)
    upload_ota(args.ota_file, args.host)
