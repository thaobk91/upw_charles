import json
import boto3
import base64


def lambda_handler(event, context):
    client = boto3.client('iot-data', region_name='us-east-1')
    device_id = event['deviceId']
    payload = event['payload']
    new_topic = f'$aws/things/{device_id}/shadow/update' 
    
    # Wrap the payload in the "reported" attribute
    wrapped_payload = {
        "device_id": device_id,
        "state": {
            "reported": payload
        }
    }
    
    # Publish the message to the new topic
    response = client.publish(
        topic=new_topic,
        qos=1,
        payload=json.dumps(wrapped_payload)
    )
            
    return {
        'statusCode': 200,
        'body': json.dumps(response)
    } 