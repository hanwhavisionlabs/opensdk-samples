# certificate_sample

This sample application demonstrates how an OpenSDK component can use the
OpenPlatform certificate APIs to load a device certificate and create a digital
signature. It registers HTTP endpoints with AppDispatcher and provides a web
page for testing them from the camera web viewer.

## Prerequisites

- A camera with Open Platform support and the certificate APIs used by this
  sample (`eLoadDeviceCert` and `eCreateDigitalSignature`)
- Access to the camera web viewer and the Open Platform menu
- Docker Compose and an available `opensdk:<SDK_VER>` image
- A `SOC` supported by both the target camera and the OpenSDK image

The available certificate features and digest algorithms depend on the camera
firmware and OpenSDK version.

## APIs

The component registers the following POST endpoints. When accessed from the
sample web page, `<APP_ID>` is `certificate_sample`.

| Operation | Endpoint | Required request fields |
| --- | --- | --- |
| Load device certificate | `/opensdk/<APP_ID>/cert/load-cert` | `to_path` |
| Create digital signature | `/opensdk/<APP_ID>/cert/sign` | `name`, `data` |

### Load Device Certificate

Request body:

```json
{
  "to_path": "/tmp/cc"
}
```

`to_path` is the directory where the platform loads the device certificate.
The application creates the directory when it does not exist, so the path must
be writable by the application.

On success, the endpoint returns the platform result:

```json
{
  "result": "<platform response>"
}
```

### Create Digital Signature

Request body:

```json
{
  "name": "default",
  "data": "Hello, Wisenet!",
  "digest": "SHA256"
}
```

`name` selects the key or certificate name used by the platform. `data` is
entered as plain text in the sample UI and is Base64-encoded by the component
before it is sent to OpenPlatform. `digest` is optional and defaults to
`SHA256`.

On success, the endpoint returns the generated signature:

```json
{
  "signature": "<signature>"
}
```

## Scenario

### Install and Start

1. Build the application and generate `certificate_sample.cap` as described in
   [Building Application](#building-application).
2. Open the camera web viewer and go to **Settings > Open Platform**.
3. Click **Install** and select the generated CAP file.
4. Click **Start** to start the application.
5. Click **Go App** in the camera web viewer.

### Load Device Certificate

1. In the **Load Device Certificate** card, enter a writable directory in
   **To Path**. The default value is `/tmp/cc`.
2. Click **Load Device Certificate**.
3. Confirm the platform result displayed below the button.

### Create Digital Signature

1. In the **Create Digital Signature** card, enter the key name. The default is
   `default`.
2. Enter the text to sign. The default is `Hello, Wisenet!`.
3. Enter a digest algorithm supported by the camera. The default is `SHA256`.
4. Click **Create Signature** and confirm that the returned signature is shown.

## HTTP Responses

- `200`: The requested certificate operation completed successfully.
- `400`: The request body is invalid or a required field is missing.
- `404`: The requested path or method is not registered by the sample.
- `500`: The OpenPlatform request timed out, returned an invalid response, or
  failed to complete.

## Building Application

Run the following commands from the `certificate_sample` directory:

```bash
APP_NAME=certificate_sample SDK_VER=<OpenSDK-image-tag> SOC=<supported-soc> docker compose up
docker compose down --remove-orphans
```

The compose workflow configures the OpenSDK toolchain, builds the CMake
project, installs the application files, and runs `opensdk_packager`. If the
build succeeds, `certificate_sample.cap` is created in this directory.

Generated build and package files such as `app/build/`, `app/bin/`,
`app/libs/`, `app/storage/`, and `*.cap` are ignored by Git. Do not commit
private keys or generated certificate material from `app/res/cert/`.
