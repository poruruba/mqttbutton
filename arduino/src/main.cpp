#include <M5Atom.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// 5x5ドットフォント
#include "bmpfont5x5.h"

// MQTT用
WiFiClient espClient;
PubSubClient client(espClient);

const char* wifi_ssid = "【WiFiアクセスポイントのSSID】";
const char* wifi_password = "【WiFiアクセスポイントのパスワード】";
const char* mqtt_server = "【MQTTブローカのIPアドレスまたはホスト名】";
const uint16_t mqtt_port = 1883; // MQTTサーバのポート番号(TCP接続)
const char* topic = "btn/m5atom"; // ボタン押下時の通知用
const char* topic_setting = "btn/m5atom_setting"; // ボタン設定受信用
#define MQTT_CLIENT_NAME  "M5Atom" // MQTTサーバ接続時のクライアント名

#define MATRIX_WIDTH  5 // LEDマトリクスの幅
#define DEFAULT_BRIGHTNESS  20  // LEDマトリクスのデフォルトの輝度
#define NUM_OF_BTNS 5   // トグル最大数
#define BTN_PUSH_MARGIN     100 // ボタン操作のチャタリング対策
#define MQTT_BUFFER_SIZE  512 // MQTT送受信のバッファサイズ

// MQTT Subscribe用
const int request_capacity = JSON_OBJECT_SIZE(4) + JSON_ARRAY_SIZE(NUM_OF_BTNS) + NUM_OF_BTNS * JSON_OBJECT_SIZE(4);
StaticJsonDocument<request_capacity> json_request;
// MQTT Publish用
const int message_capacity = JSON_OBJECT_SIZE(3);
StaticJsonDocument<message_capacity> json_message;
char message_buffer[MQTT_BUFFER_SIZE];

typedef enum {
  IDLE = 0, // 何もしない
  CLICK, // 押下時に通知
  TOGGLE, // 押下時に通知＋状態維持
} INPUT_MODE; // ボタン動作モード
typedef struct {
  CRGB fore; // 文字色
  CRGB back; // 背景色
  char ch; // 表示文字
  uint8_t rotate; // 回転(0～3)
} BTN_VIEW; // 表示ボタン

const CRGB default_fore(255, 255, 255); // デフォルト文字色
const CRGB default_back(0, 0, 0); // デフォルト背景色
#define DEFAULT_CHAR  ' ' // デフォルト表示文字
#define DEFAULT_ROTATE  0 //デフォルト回転
// 表示ボタンのリスト(デフォルト値含む)
BTN_VIEW btns[NUM_OF_BTNS] = {
  {
    default_fore,
    default_back,
    DEFAULT_CHAR,
    0,
  }
};
uint8_t num_of_btns = 1; // 有効な表示ボタンの数

INPUT_MODE current_mode = CLICK; // 現在のボタン動作モード
unsigned long current_id = 0; // 現在のボタン設定のID

bool mode_reset = false; // MQTT Subscribeで新しい設定を受信しているかどうか
uint8_t current_btn_index = 0; // 現在の表示ボタンのインデックス
bool pressed = false;

// MQTT Subscribeで受信した際のコールバック関数
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("received");
  if( mode_reset ){
    // まだ以前に受信したボタン設定が未設定の場合は無視
    Serial.println("Mode change Busy");
    return;
  }

  DeserializationError err = deserializeJson(json_request, payload, length);
  if( err ){
    Serial.println("Deserialize error");
    Serial.println(err.c_str());
    return;
  }

  // 新しいボタン状態を受信したことをセット
  mode_reset = true;
}

// WiFiアクセスポイントへの接続
void wifi_connect(void){
  Serial.println("");
  Serial.print("WiFi Connenting");
  M5.dis.clear();

  uint8_t index = 0;
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    M5.dis.drawpix(index++, default_fore);
    if( index >= MATRIX_WIDTH * MATRIX_WIDTH ){
      M5.dis.clear();
      index = 0;
    }
    delay(1000);
  }
  Serial.println("");
  Serial.print("Connected : ");
  Serial.println(WiFi.localIP());
  M5.dis.clear();

  // バッファサイズの変更
  client.setBufferSize(MQTT_BUFFER_SIZE);
  // MQTTコールバック関数の設定
  client.setCallback(mqtt_callback);
  // MQTTブローカに接続
  client.setServer(mqtt_server, mqtt_port);
}

// 表示ボタンのインデックスのインクリメント
void incrementIndex(void){
  if( current_btn_index + 1 >= num_of_btns )
    current_btn_index = 0;
  else
    current_btn_index++;
}

// LEDマトリクスへ文字の表示
void setText(char ch, CRGB fore, CRGB back, uint8_t rotate = 0, bool invert = false){
  if( ch >= ' ' && ch <= '~' ){
    const uint8_t *t = &bmpfont_5x5[(ch - ' ') * MATRIX_WIDTH];
    uint8_t buffer[2 + 3 * MATRIX_WIDTH * MATRIX_WIDTH] = { MATRIX_WIDTH, MATRIX_WIDTH };
    for( int y = 0 ; y < MATRIX_WIDTH ; y++ ){
      for( int x = 0 ; x < MATRIX_WIDTH ; x++ ){
        // 回転後のポジションの算出
        int pos;
        if( rotate == 0 )
          pos = 2 + (y * MATRIX_WIDTH + x) * 3; 
        else if( rotate == 1 )
          pos = 2 + (x * MATRIX_WIDTH + (MATRIX_WIDTH - y - 1)) * 3;
        else if( rotate == 2 )
          pos = 2 + ((MATRIX_WIDTH - y - 1) * MATRIX_WIDTH + (MATRIX_WIDTH - x - 1)) * 3;
        else
          pos = 2 + ((MATRIX_WIDTH - x - 1) * MATRIX_WIDTH + y) * 3;

        // 文字色の設定
        CRGB color = ( t[y] & (0x01 << (MATRIX_WIDTH - x - 1)) ) ? fore : back;
        buffer[pos + 0] = color.r;
        buffer[pos + 1] = color.g;
        buffer[pos + 2] = color.b;
        if( invert ){
          // invert==true の場合に減色
          buffer[pos + 0] /= 3;
          buffer[pos + 1] /= 3;
          buffer[pos + 2] /= 3;
        }
      }
    }
    // LEDマトリクスに表示
    M5.dis.displaybuff(buffer, 0, 0);
  }else{
    // サポートしない文字の場合は表示を消去
    M5.dis.clear();
  }
}

// 現在の表示ボタンインデックスに合わせて文字を表示
void showButton(bool invert){
  setText(btns[current_btn_index].ch, btns[current_btn_index].fore, btns[current_btn_index].back, btns[current_btn_index].rotate, invert);
}

// MQTTで受信したボタン設定を解析し設定
void parseRequest(void){
  serializeJson(json_request, Serial);
  Serial.println("");

  current_mode = json_request["mode"];
  current_id = json_request["id"] | 0;
  uint8_t brightness = json_request["brightness"] | DEFAULT_BRIGHTNESS;
  M5.dis.setBrightness(brightness);
  JsonArray array = json_request["btns"].as<JsonArray>();
  num_of_btns = array.size();
  if( num_of_btns > NUM_OF_BTNS )
    num_of_btns = NUM_OF_BTNS;

  for( int i = 0 ; i < num_of_btns ; i++ ){
    uint32_t color;
    if( array[i]["fore"] ){
      color = array[i]["fore"];
      btns[i].fore = color;
    }else{
      btns[i].fore = default_fore;
    }
    if( array[i]["back"] ){
      color = array[i]["back"];
      btns[i].back = color;
    }else{
      btns[i].back = default_back;
    }
    const char* str = array[i]["char"];
    btns[i].ch = str[0];
    btns[i].rotate = array[i]["rotate"] | DEFAULT_ROTATE;
  }
}

// ボタン押下イベントをMQTTで通知
void sendMessage(void){
  json_message.clear();
  json_message["mode"] = current_mode;
  json_message["id"] = current_id;
  if( current_mode == TOGGLE )
    json_message["index"] = current_btn_index;

  serializeJson(json_message, message_buffer, sizeof(message_buffer));
  client.publish(topic, message_buffer);
}

void setup() {
  M5.begin(true, false, true);
  M5.dis.setBrightness(DEFAULT_BRIGHTNESS);

  Serial.begin(9600);
  wifi_connect();
  showButton(pressed);
}

void loop() {
  client.loop();
  // MQTT未接続の場合、再接続
  while(!client.connected() ){
    Serial.println("Mqtt Reconnecting");
    if( client.connect(MQTT_CLIENT_NAME) ){
      // ボタン再表示
      showButton(pressed);
      // MQTT Subscribe
      client.subscribe(topic_setting);
      Serial.println("Mqtt Connected and Subscribing");
      break;
    }
    delay(1000);
  }

  M5.update();

  if(mode_reset){
    // ボタン設定受信後の処理
    parseRequest();
    mode_reset = false;
    current_btn_index = 0;
    pressed = false;
    // ボタン再表示
    showButton(pressed);
  }

  if( current_mode != IDLE ){
    if (M5.Btn.isPressed() && !pressed ){
      pressed = true;
      // ボタン再表示
      showButton(pressed);
      if( current_mode == TOGGLE )
        incrementIndex(); // TOGGLEの場合、表示ボタンを切り替え
      // ボタン押下イベントを送信
      sendMessage();

      delay(BTN_PUSH_MARGIN);
    }else if( M5.Btn.isReleased() && pressed ){
      pressed = false;
      // ボタン再表示
      showButton(pressed);

      delay(BTN_PUSH_MARGIN);
    }
  }
}
