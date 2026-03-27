# Cellular buidling monitor app

## Requirements

- nRF Connect SDK v2.6.2
- Noridc nrf9160 devices mfw 1.3.7
- Nordic nrf9151 devices mfw 2.02

## MCU boot key

- MCUBoot bootloader key file: ncs/v2.6.2/bootloader/mcuboot/root-ec-p256.pem (default)

## Testing

- Update modem firmware.
- Flash aws claim certificates on the tag **204**.
- Add Build configuration for the board _nowi_cellular_ns_.
- Flash the app.

## Downlinks

Topic: **$aws/things/`<deviceID>`/shadow/update**

Example:

```json
{
  "fcnt": 0,
  "state": {
    "reported": {
      "dev": {
        "v": {
          "devid": "333A99E15AE61897",
          "iccid": "89882280666087469320",
          "imei": "351901930759778",
          "fwv": "v0.2.5",
          "algo": "FOLLOWER",
          "log": 1
        },
        "ts": 1721654687
      },
      "roam": {
        "v": {
          "rsrp": -87,
          "bat": 5023,
          "psmen": 1
        },
        "ts": 1721654687360
      },
      "cfg": {
        "udc": 600,
        "adc": 60
      },
      "env": [
        {
          "hc": 0,
          "algo_cfg": 0,
          "ts": 1721654612
        },
        {
          "hc": 7,
          "algo_cfg": 0,
          "ts": 1721654672
        }
      ],
      "alrt": [
        {
          "ts": 1721654612,
          "error_code": 176,
          "version": 0,
          "data": [3, 0]
        }
      ]
    }
  }
}
```

```json
{
  "state": {
    "cfg": {
      "udc": 600,
      "adc": 60,
      "avrs": {
        "maxNoise_mG": 1
      }
    }
  }
}
```

_psmen_ - PSM enabled/disabled flag (value 0 or 1).
_udc_ - upload duty cycle in seconds (value should be >= MIN*UPLOAD_DUTY_CYCLE_SECONDS).
\_adc* - app dudty cycle in seconds (value should be >= MIN*APP_DUTY_CYCLE_SECONDS).
\_avrs* - algorithm variables changes. When included, it is expected to have any of the following variables. Values handled as integgers.
_percentSignalThreshold_
_percentNoiseThreshold_
_percentNoiseBuffer_
_maxNoise_mG_
_minNoise_mG_
_monitorMissedCycles_
_staticNoiseBuffer_
_minNoiseChange_
_percentOfNoiseDecreaseForReset_
_maxNumberOfTimestampsToAverage_
_maxTimeSinceLastPulse_
_maxSignalFrequency_
_debugAlerts_
_gettingData_
_minConfigurationDifference_

Topic: **nowi/cellular/devices/`<deviceID>`/downlink**
Example:

```json
{
  "lnkact": "psmen",
  "apact": "start"
}
```

_lnkact_: LTE link action. Supported actions: _"psmen"_ and _"psmdis"_.
_apct_: Application action. Supported actions: _"reset"_, _"reboot"_, "_start_" and _"stop"_.

# Instructions

## Provisioning And Programming

Before flashing the certificates, the nrf91 app should have the AT commands enabled, to not just enabled that on the firmware you could just flash the at_client app hex code that comes with the (application and modem firmware) package.

Update Modem firmware

- Use programmer nordic app
- Use the programmer (not UART)
- Load the mfw\*\* \*\*

Loading Certificates

- [https://devzone.nordicsemi.com/guides/cellular-iot-guides/b/software-and-protocols/posts/connecting-to-aws-cloud-services-using-the-nrf9160](https://devzone.nordicsemi.com/guides/cellular-iot-guides/b/software-and-protocols/posts/connecting-to-aws-cloud-services-using-the-nrf9160)
- Use USB port UART
- Flashing certificates is done using the LTE link monitor tool on the nRF connect desktop app, there is a section called CERTIFICATE MANAGER.
- The certificates needs to be assigned a security tag, we set that to 204.
- the modem needs to be on offline mode AT+CFUN=4.

Certificates

- CA = AmazonRootCA1.pem
- Private = Private
- Client = other certificate in folder
- Dismiss public certificate

Flashing:

- Connect board to programmer and flash in VS code or with programmer

## FOTA Instructions

Create an S3 bucket

- The bucked should have two files:
  - app_update.bin : This file is generated when building the app to update via FOTA, and it is located in the **build/zephyr** folder.
  - fota_job.txt : a text files where FOTA parameters can be set.
- An example:
  - [https://s3.console.aws.amazon.com/s3/buckets/nrf91-fotas?region=us-east-1&amp;tab=objects](https://s3.console.aws.amazon.com/s3/buckets/nrf91-fotas?region=us-east-1&tab=objects)

To send the update, a job should be created on AWS Iot > Manage > Remote actions > Jobs.

- [https://us-east-1.console.aws.amazon.com/iot/home?region=us-east-1#/jobhub](https://us-east-1.console.aws.amazon.com/iot/home?region=us-east-1#/jobhub)

Create a custom job, give it a name, select the icarus91 on the Things to run this job on, Browse S3 and pick the fota_job.txt file, and select Snapshot as the job type.

- [s3://nrf91-fotas/fota_job.txt](s3://nrf91-fotas/fota_job.txt)
- Size and version don’t matter right now
- This should create the job and roll it out.
- The update could take several minutes to be sent the device. The job status should be updated accordingly.

Example: fota_job.txt

```json
{
  "operation": "app_fw_update",
  "fwversion": "v0.0.1",
  "size": 181124,
  "location": {
    "protocol": "https",
    "host": "nrf91-fotas.s3.amazonaws.com",
    "path": "app_update.bin"
  }
}
```

## protbuf

To Install libraries to generate protobuf schema, go to src/protobuf folder

```
pip install -r requirements.txt

```

The filesdescriptor should be generated using the following command:

```bash
protoc --include_imports -o filedescriptor.desc msg.proto
```

file descriptor (filedescriptor.desc) is saved in **nowi-protobuf-fildesc** bucket **/msg** folder on S3.

Protobuf encoded messages are routed via **CellularDeviceUplinkRouting** rule to **nowi-protobuf-decode-route** lambda function. Rule consists of the following SQL statement:

```sql
SELECT decode(encode(*, 'base64'), "proto", "nowi-protobuf-filedesc", "msg/filedescriptor.desc", "msg", "ShadowUpdate") as payload, topic(4) as deviceId
FROM 'nowi/cellular/devices/+/uplink'
```

The lambda funtion basically just routes the decoded message to the shadow **$aws/things/{device_id}/shadow/update** topic.

[How to Article for Protobuf](https://aws.amazon.com/blogs/iot/how-to-build-smart-applications-using-protocol-buffers-with-aws-iot-core/ "How to Article for Protobuf")
