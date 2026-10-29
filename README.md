# OpenSDK Samples

Sample applications for the Hanwha Vision OpenSDK.
Each sample shows how to build an application that runs on Hanwha Vision cameras, using the OpenSDK build environment distributed as a Docker image.

## Prerequisites

- Docker
- A Hanwha Vision camera that supports OpenSDK applications <!-- TODO: link to the supported devices list -->
- The OpenSDK Docker image for your target platform:

| Platform | Image |
|---|---|
| WN9 | [hanwhavisionlabs/opensdk-wn9](https://hub.docker.com/r/hanwhavisionlabs/opensdk-wn9) |

```bash
docker pull hanwhavisionlabs/opensdk-wn9:<version>
```

## Samples

| Sample | Description |
|---|---|
| <!-- TODO: sample directory --> | <!-- TODO: one-line description --> |

Each sample directory contains its own README with build and deployment instructions.

## Getting Started

1. Clone this repository.

```bash
   git clone https://github.com/hanwhavisionlabs/opensdk-samples.git
   cd opensdk-samples
```

2. Start the OpenSDK build container with the repository mounted.

```bash
   docker run --rm -it \
     -v "$(pwd)":/workspace \
     -w /workspace \
     hanwhavisionlabs/opensdk-wn9:<version>
```

3. Build a sample inside the container.

```bash
   cd <sample-directory>
   # TODO: build command
```

4. Install the built application on your camera. <!-- TODO: install method or link to the docs -->

## Documentation

For programming guides and API references, see the [OpenSDK Documentation](TODO: opensdk-docs URL).

## Support

If you find a bug in a sample, please open an [issue](https://github.com/hanwhavisionlabs/opensdk-samples/issues).

## License

Copyright (c) 2026 Hanwha Vision Co., Ltd.

Licensed under the TODO: license name. See [LICENSE](LICENSE) for details.
