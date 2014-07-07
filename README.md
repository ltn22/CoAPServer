This a constraint implementation of CoAP for Arduino. It can run on a Arduino UNO. Currently the CoAP server implemnt GET, PUT and partially POST (no resource creation). In the example directory, the CoAPServertestlib shows an example of a server using CoAP. A photoresistor in pin A0 increment the value nbFlash after a blink has been detect. The CoAP server is used to propagate this value.

CoAPServer.h constaints the define used in the program and the structure definitions:
- DUMP_COAP when defined makes the  program sending some debuging info on the Serial link. To gain space it is recommended not to use it
- COAP_SERVER_ETHERNET when defined also the servur to run on the Ethernet port. Currently this value is not used, but in the furtur COAP_SERVER_WIFI and COAP_SERVER_I2C will be used to allow communication on other media.
- MAX_OBSERVE specifies how many simultaneous Observe can be processed by the server, the more are defined, the more memory it will take. By default the value is 2
- URI_LENGTH specifies the size of the URI path the server can process. By default the size is 20 characters including /.

An EthernetUdp handler is created as usual on port 5683 (default port for CoAP). When the Udp object handler is created, a CoAP server object (CS) is initialized with this handler throught the begin method.

Resources are then associated with this server. In the example a resource for the /flash resource is created through the addRes method. This method has 3 syntax:
- addRes(String, function_get)
- addRes(String, function_put)
- addRes(String, function_get, function_put)

String contains the resource name, function_get and function_put are two functions called when the CoAP server will receive a request on that resource. Note that if function_get is not defined, then the server will answer a error message when a get request is received, same thing for the put. The method return an handler on the resource. It will be useful in the rest of the program to identify the resource whitout string comparison.

Function_get has the following syntax:

String function_get (CoAPResource* res, uint8_t format)

res is the handler of the resource and format is the MIME type in CoAP representation (COAP_FORMAT_TEXT, COAP_FORMAT_XML and COAP_FORMAT_JSON are currently defined, but other values can be used as well). This function returns a string which will be sent to the client. 

Function_put has the following syntax:

uint8_t function_put (CoAPResource res, uint8_t format, uint8_t method, String input)

The first two parameters are the same as function_get, method allows to distinguish between PUT and POST. The current library does not allow dynamic resource creation, but two behaviors can be defined. In this example a POST will set the flash counter to 0 and PUT will set it to a specific value. 

In the loop the method incoming has to be called. This method returns after processing requests, it also manage periodic sending of observe messages. 
