#include <assert.h>

#define AZURE_IOT_CENTRAL_DPS_ENDPOINT "global.azure-devices-provisioning.net"
#define TEMP_BUFFER_SIZE 1024
#define AUTH_BUFFER_SIZE 256

int getDPSAuthString(char* scopeId, char* deviceId, char* key, char *buffer, int bufferSize, size_t &outLength) {
  unsigned long expiresSecond = rtc.getEpoch() + 7200;
  assert(expiresSecond > 7200);

  String deviceIdEncoded = urlEncode(deviceId);
  char dataBuffer[AUTH_BUFFER_SIZE] = {0};
  size_t size = snprintf(dataBuffer, AUTH_BUFFER_SIZE, "%s%%2Fregistrations%%2F%s", scopeId, deviceIdEncoded.c_str());
  assert(size < AUTH_BUFFER_SIZE); dataBuffer[size] = 0;
  String sr = dataBuffer;
  size = snprintf(dataBuffer, AUTH_BUFFER_SIZE, "%s\n%lu000", sr.c_str(), expiresSecond);
  const size_t dataBufferLength = size;
  assert(dataBufferLength < AUTH_BUFFER_SIZE); dataBuffer[dataBufferLength] = 0;

  char keyDecoded[AUTH_BUFFER_SIZE] = {0};
  size = base64_decode(keyDecoded, key, strlen(key));
  assert(size < AUTH_BUFFER_SIZE && keyDecoded[size] == 0);
  const size_t keyDecodedLength = size;

  Sha256 *sha256 = new Sha256();
  sha256->initHmac((const uint8_t*)keyDecoded, (size_t)keyDecodedLength);
  sha256->print(dataBuffer);
  char* sign = (char*) sha256->resultHmac();
  int encodedSignLen = base64_enc_len(HASH_LENGTH);
  char encodedSign[encodedSignLen];
  base64_encode(encodedSign, sign, HASH_LENGTH);
  delete(sha256);

  String auth = urlEncode(encodedSign);
  outLength = snprintf(buffer, bufferSize, "authorization: SharedAccessSignature sr=%s&sig=%s&se=%lu000&skn=registration", sr.c_str(), auth.c_str(), expiresSecond);
  buffer[outLength] = 0;

  return 0;
}

int _getOperationId(char* scopeId, char* deviceId, char* authHeader, char *operationId) {
  WiFiSSLClient client;
  if (client.connect(AZURE_IOT_CENTRAL_DPS_ENDPOINT, 443)) {
    char tmpBuffer[TEMP_BUFFER_SIZE] = {0};
    String deviceIdEncoded = urlEncode(deviceId);
    size_t size = snprintf(tmpBuffer, TEMP_BUFFER_SIZE,
      "PUT /%s/registrations/%s/register?api-version=2018-11-01 HTTP/1.0", scopeId, deviceIdEncoded.c_str());
    assert(size != 0); tmpBuffer[size] = 0;
    client.println(tmpBuffer);
    client.println("Host: global.azure-devices-provisioning.net");
    client.println("content-type: application/json; charset=utf-8");
    client.println("user-agent: iot-central-client/1.0");
    client.println("Accept: */*");
    size = snprintf(tmpBuffer, TEMP_BUFFER_SIZE,
      "{\"registrationId\":\"%s\"}", deviceId);
    assert(size != 0); tmpBuffer[size] = 0;
    String regMessage = tmpBuffer;
    size = snprintf(tmpBuffer, TEMP_BUFFER_SIZE,
      "Content-Length: %d", regMessage.length());
    assert(size != 0); tmpBuffer[size] = 0;
    client.println(tmpBuffer);

    client.println(authHeader);
    client.println("Connection: close");
    client.println();
    client.println(regMessage.c_str());

    delay(2000); // give 2 secs to server to process
    memset(tmpBuffer, 0, TEMP_BUFFER_SIZE);
    int index = 0;
    while (client.available() && index < TEMP_BUFFER_SIZE - 1) {
      tmpBuffer[index++] = client.read();
    }
    tmpBuffer[index] = 0;
    const char* operationIdString= "{\"operationId\":\"";
    index = indexOf(tmpBuffer, TEMP_BUFFER_SIZE, operationIdString, strlen(operationIdString), 0);
    if (index == -1) {
error_exit:
      Serial.println("ERROR: Error from DPS endpoint");
      Serial.println(tmpBuffer);
      return 1;
    } else {
      index += strlen(operationIdString);
      int index2 = indexOf(tmpBuffer, TEMP_BUFFER_SIZE, "\"", 1, index + 1);
      if (index2 == -1) goto error_exit;
      tmpBuffer[index2] = 0;
      strcpy(operationId, tmpBuffer + index);
      // Serial.print("OperationId:");
      // Serial.println(operationId);
      client.stop();
    }
  } else {
    Serial.println("ERROR: Couldn't connect AzureIOT DPS endpoint.");
    return 1;
  }

  return 0;
}

int _getHostName(char *scopeId, char*deviceId, char *authHeader, char*operationId, char* hostName) {
  WiFiSSLClient client;
  if (!client.connect(AZURE_IOT_CENTRAL_DPS_ENDPOINT, 443)) {
    Serial.println("ERROR: DPS endpoint GET call has failed.");
    return 1;
  }
  char tmpBuffer[TEMP_BUFFER_SIZE] = {0};
  String deviceIdEncoded = urlEncode(deviceId);
  size_t size = snprintf(tmpBuffer, TEMP_BUFFER_SIZE,
    "GET /%s/registrations/%s/operations/%s?api-version=2018-11-01 HTTP/1.1", scopeId, deviceIdEncoded.c_str(), operationId);
  assert(size != 0); tmpBuffer[size] = 0;
  client.println(tmpBuffer);
  client.println("Host: global.azure-devices-provisioning.net");
  client.println("content-type: application/json; charset=utf-8");
  client.println("user-agent: iot-central-client/1.0");
  client.println("Accept: */*");
  client.println(authHeader);
  client.println("Connection: close");
  client.println();
  delay(5000); // give 5 secs to server to process
  memset(tmpBuffer, 0, TEMP_BUFFER_SIZE);
  int index = 0;
  while (client.available() && index < TEMP_BUFFER_SIZE - 1) {
    tmpBuffer[index++] = client.read();
  }
  tmpBuffer[index] = 0;
  const char* lookFor = "\"assignedHub\":\"";
  index = indexOf(tmpBuffer, TEMP_BUFFER_SIZE, lookFor, strlen(lookFor), 0);
  if (index == -1) {
    Serial.println("ERROR: couldn't get assignedHub. Trying again..");
    Serial.println(tmpBuffer);
    return 2;
  }
  index += strlen(lookFor);
  int index2 = indexOf(tmpBuffer, TEMP_BUFFER_SIZE, "\"", 1, index + 1);
  memcpy(hostName, tmpBuffer + index, index2 - index);
  hostName[index2-index] = 0;
  client.stop();
  return 0;
}

int getHubHostName(char *scopeId, char* deviceId, char* key, char *hostName) {
  char authHeader[AUTH_BUFFER_SIZE] = {0};
  size_t size = 0;
  //Serial.println("- iotc.dps : getting auth...");
  if (getDPSAuthString(scopeId, deviceId, key, (char*)authHeader, AUTH_BUFFER_SIZE, size)) {
    Serial.println("ERROR: getDPSAuthString has failed");
    return 1;
  }
  //Serial.println("- iotc.dps : getting operation id...");
  char operationId[AUTH_BUFFER_SIZE] = {0};
  if (_getOperationId(scopeId, deviceId, authHeader, operationId) == 0) {
    delay(4000);
    //Serial.println("- iotc.dps : getting host name...");
    while( _getHostName(scopeId, deviceId, authHeader, operationId, hostName) == 2) delay(5000);
    return 0;
  }
}