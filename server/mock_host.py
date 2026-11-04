"""Print commands for a demo; never changes the operating-system workspace."""
import argparse
import asyncio
import json

from websockets.asyncio.client import connect


async def main(url):
    async with connect(url) as socket:
        print(f"Mock host connected to {url}", flush=True)
        async for message in socket:
            print(json.dumps(json.loads(message), indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="ws://127.0.0.1:8000/ws/hosts/host-01")
    asyncio.run(main(parser.parse_args().url))
