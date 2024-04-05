#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <ArduinoJson.h>

#define LED 2 //LED灯连接到GPIO2,用LED灯指示设备状态


#define product_id " " //产品ID，改为自己的产品ID
#define device_id " " //设备ID，改为自己的设备ID
#define token " " //token，改为自己的token

const char* ssid = " ";//WiFi名称，改为自己的WiFi名称
const char* password = " ";//WiFi密码，改为自己的WiFi密码



const char* mqtt_server = "mqtts.heclouds.com";//MQTT服务器地址
const int mqtt_port = 1883;//MQTT服务器端口

#define ONENET_TOPIC_PROP_POST "$sys/" product_id "/" device_id "/thing/property/post"
//设备属性上报请求,设备---->OneNET
#define ONENET_TOPIC_PROP_SET "$sys/" product_id "/" device_id "/thing/property/set"
//设备属性设置请求,OneNET---->设备
#define ONENET_TOPIC_PROP_POST_REPLY "$sys/" product_id "/" device_id "/thing/property/post/reply"
//设备属性上报响应,OneNET---->设备
#define ONENET_TOPIC_PROP_SET_REPLY "$sys/" product_id "/" device_id "/thing/property/set_reply"
//设备属性设置响应,设备---->OneNET
#define ONENET_TOPIC_PROP_FORMAT "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":%s}"
//设备属性格式模板
int postMsgId = 0;//消息ID,消息ID是需要改变的,每次上报属性时递增

//按照自己的设备属性定义，可以通过DHT11传感器获取温湿度，也可以通过开关控制LED，这里只是模拟
float temp = 28.0;//温度
int humi = 60;//湿度
bool LED_Status = false;//LED状态


WiFiClient espClient;//创建一个WiFiClient对象
PubSubClient client(espClient);//创建一个PubSubClient对象
Ticker ticker;//创建一个定时器对象

void LED_Flash(int time);
void WiFi_Connect();
void OneNet_Connect();
void OneNet_Prop_Post();
void callback(char* topic, byte* payload, unsigned int length);

void setup() {
	pinMode(LED, OUTPUT);//LED灯设置为输出模式
	Serial.begin(9600);//串口初始化,波特率9600,用于输出调试信息，这里串口波特率要与串口监视器设置的一样，否则会乱码
	WiFi_Connect();//连接WiFi
	OneNet_Connect();//连接OneNet
	ticker.attach(10, OneNet_Prop_Post);//定时器,每10ms执行一次OneNet_Prop_Post函数
}

void loop() {
	if(WiFi.status() != WL_CONNECTED) {//如果WiFi连接断开
		WiFi_Connect();//重新连接WiFi
	}
	if (!client.connected()) {//如果MQTT连接断开
		OneNet_Connect();//重新连接OneNet
	}
	client.loop();//保持MQTT连接
}

void LED_Flash(int time) {
	digitalWrite(LED, HIGH);//点亮LED
	delay(time);//延时time
	digitalWrite(LED, LOW);//熄灭LED
	delay(time);//延时time
}

void WiFi_Connect()
{
	WiFi.begin(ssid, password);//连接WiFi
	while (WiFi.status() != WL_CONNECTED) {//等待WiFi连接,WiFI.status()返回当前WiFi连接状态,WL_CONNECTED为连接成功状态
		LED_Flash(500);//LED闪烁,循环等待
		Serial.println("\nConnecting to WiFi...");
	}
	Serial.println("Connected to the WiFi network");//WiFi连接成功
	Serial.println(WiFi.localIP());//输出设备IP地址
	digitalWrite(LED, HIGH);//点亮LED,表示WiFi连接成功
}

void OneNet_Connect()
{
	client.setServer(mqtt_server, mqtt_port);//设置MQTT服务器地址和端口
	client.connect(device_id, product_id, token);//连接OneNet
	if(client.connected()) //如果连接成功
	{
		LED_Flash(500);
		Serial.println("Connected to OneNet!");
	}
	else
	{
		Serial.println("Failed to connect to OneNet!");
	}
	client.subscribe(ONENET_TOPIC_PROP_SET);//订阅设备属性设置请求, OneNET---->设备
	client.subscribe(ONENET_TOPIC_PROP_POST_REPLY);//订阅设备属性上报响应,OneNET---->设备
	client.setCallback(callback);//设置回调函数
}
//上报设备属性
void OneNet_Prop_Post()
{
	if(client.connected()) 
	{
		char parmas[256];//属性参数
		char jsonBuf[256];//JSON格式数据,用于上报属性的缓冲区
		sprintf(parmas, "{\"temp\":{\"value\":%.1f},\"humi\":{\"value\":%d},\"LED\":{\"value\":%s}}", temp, humi, LED_Status ? "true" : "false");//设置属性参数
		Serial.println(parmas);
		sprintf(jsonBuf,ONENET_TOPIC_PROP_FORMAT,postMsgId++,parmas);//设置JSON格式数据,包括消息ID和属性参数
		Serial.println(jsonBuf);
		if(client.publish(ONENET_TOPIC_PROP_POST, jsonBuf))//上报属性
		{
			LED_Flash(500);
			Serial.println("Post property success!");
		}
		else
		{
			Serial.println("Post property failed!");
		}
	}
}
//回调函数，当订阅的主题有消息时，会调用此函数
void callback(char* topic, byte* payload, unsigned int length)
{
	Serial.print("Message arrived [");
	Serial.print(topic);//打印主题
	Serial.print("] ");
	for (int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);//打印消息内容
	}
	Serial.println();//换行
	LED_Flash(500);//LED闪烁,表示收到消息
	if(strcmp(topic, ONENET_TOPIC_PROP_SET) == 0)
	{
		DynamicJsonDocument doc(100);//创建一个JSON文档,用于解析消息内容

		DeserializationError error = deserializeJson(doc, payload);//解析消息内容,将消息内容存储到doc中,返回解析结果,成功返回0,失败返回其他值
		if (error) {//解析失败,打印错误信息,返回
			Serial.print(F("deserializeJson() failed: "));
			Serial.println(error.c_str());
			return;
		}
		JsonObject setAlinkMsgObj = doc.as<JsonObject>();//获取JSON文档的根对象
		JsonObject params = setAlinkMsgObj["params"];//获取params对象
		if(params.containsKey("LED"))//判断params对象是否包含LED属性
		{
			LED_Status = params["LED"];//获取LED属性值
			Serial.print("Set LED:");
			Serial.println(LED_Status);
		}
		serializeJsonPretty(setAlinkMsgObj, Serial);//打印JSON文档
		String str = setAlinkMsgObj["id"];//获取消息ID
		char SendBuf[100];//发送缓冲区
		sprintf(SendBuf, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", str.c_str());//设置响应消息
		Serial.println(SendBuf);
		delay(100);
		if(client.publish(ONENET_TOPIC_PROP_SET_REPLY, SendBuf))//发送响应消息,不知道为什么，这里发送成功的，但是OneNET平台上没有收到
		{
			Serial.println("Send set reply success!");
		}
		else
		{
			Serial.println("Send set reply failed!");
		}
	}
}