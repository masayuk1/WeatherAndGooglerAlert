#include <HTTPClient.h>
#include <M5StickC.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "imgs.c"

#include <esp8266-google-home-notifier.h>

const char* ssid     = "test";   // your network SSID (name of wifi network)
const char* password = "test";    // your network password
const String citycode = "test";//天気を取得する地域コード(270000は大阪全域)

GoogleHomeNotifier ghn;
const char displayName[] = "test"; //スマホアプリで確認。Google Home のデバイス名

const byte SUNNY = 0;
const byte CLOUDY = 1;
const byte RAINY = 2;
const byte MAIL = 3;

byte AM_W;//午前の天気
byte PM_W;//午後の天気
String DATE = "";//画面に表示する日付文字列

unsigned int counter;//ループ回数のカウンタ


void updateWeather() { //Webから天気を取ってAM_W,PM_Wを更新
  HTTPClient http;
  String url = "http://weather.livedoor.com/forecast/webservice/json/v1?city=" + citycode;
  http.begin(url);
  int httpCode = http.GET();//GETメソッドで接続
  if (httpCode == HTTP_CODE_OK) {//正常にGETができたら
    String payload = http.getString();
    payload.replace("\\", "¥"); //JSONレスポンス内の"\"をエスケープしないとデシリアライズが失敗する

    DynamicJsonDocument doc(7000);//レスポンスデータのJSONオブジェクトを格納する領域を確保
    deserializeJson(doc, payload); //HTTPのレスポンス文字列をJSONオブジェクトに変換
    JsonVariant today = doc["forecasts"][0];//forecastsプロパティの0番目の要素が今日の天気に関するプロパティ
    DATE = today["date"].as<String>();//JSONから日付文字列を取得
    String telop = today["telop"];//予報文字列を取得("晴"、"曇り"、"曇のち雨"等)、ただしUnicode形式になっている

    int isFollow = telop.indexOf("¥u306e¥u3061");//予報文字列内の"のち"の位置を取得(なければ-1)
    if (isFollow < 0) { //"のち"がないなら午前も午後も同じ天気にする
      AM_W = decodeStr2Weather(telop);
      PM_W = AM_W;
    }
    else { //"のち"があるなら前後の文字列をそれぞれ午前・午後の天気にする
      telop.replace("¥u306e¥u3061", ""); //"のち"を除去
      AM_W = decodeStr2Weather(telop.substring(0, isFollow));
      PM_W = decodeStr2Weather(telop.substring(isFollow, telop.length()));
    }
  } 
  else { //GETが失敗したらフェールセーフとして雨を設定する
    AM_W = RAINY;
    PM_W = RAINY;
    DATE = "nodata";
  }
  http.end();
}

byte decodeStr2Weather(String str) { //文字列を天気定数に変換
  if (str.indexOf("¥u96e8") >= 0) //"雨"の字が含まれるなら
    return RAINY;
  else if (str.indexOf("¥u66c7") >= 0) //"曇"の字が含まれるなら
    return CLOUDY;
  else if (str.indexOf("¥u6674") >= 0) //"晴"の字が含まれるなら
    return SUNNY;
  else//上記の字が含まれないならフェイルセーフとして雨を返す
    return RAINY;
}

void drawWeather() { //AM_W,PM_Wの値に基づき画面に天気を描画
  M5.Lcd.setTextDatum(0);//文字描画の際の原点を左上に設定
  M5.Lcd.setTextColor(WHITE, 0x0000);//文字色を白、背景色を黒に設定
  M5.Lcd.drawString(DATE, 4, 0);//日付文字列を描画
  M5.Lcd.drawBitmap(8, 12, 64, 64, w_icon[AM_W]);//AMの天気アイコンを描画
  M5.Lcd.drawBitmap(88, 12, 64, 64, w_icon[PM_W]);//PMの天気アイコンを描画
}

void googleHomeConnection( String lang_str1, String talk_str1 ){
  Serial.println("connecting to Google Home...");
  if (ghn.device( displayName, lang_str1.c_str() ) != true) {
    Serial.println(ghn.getLastError());
    return;
  }
  Serial.print("found Google Home(");
  Serial.print(ghn.getIPAddress());
  Serial.print(":");
  Serial.print(ghn.getPort());
  Serial.println(")");
 
  if (ghn.notify( talk_str1.c_str() ) != true) {
    Serial.println(ghn.getLastError());
    return;
  }
  Serial.println("Done.");
}

void setup() {
  M5.begin();//M5StickCオブジェクトを初期化
  M5.Axp.ScreenBreath(8);//画面輝度を設定(7~15)
  M5.Lcd.setRotation(3);//画面を横向きに

  M5.Lcd.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, password);//WiFiへの接続開始
  while (WiFi.status() != WL_CONNECTED) {
    M5.Lcd.print(".");
    delay(200);//接続待ち遅延
  }
  M5.Lcd.println("Connected!");
  delay(1500);

  M5.Lcd.fillScreen(0x0000);//画面を黒で塗りつぶし
  counter = 0;//電源投入時を基準に1[s]ごとにカウントアップされていく
}

void loop() {
  //画面中央の▲を2[s]周期で点滅表示
  if (counter % 2 == 0)
    M5.Lcd.fillTriangle(74, 40 - 16, 74, 40 + 16, 86, 40, GREEN); //緑色
  else
    M5.Lcd.fillTriangle(74, 40 - 16, 74, 40 + 16, 86, 40, 0x0000); //黒色

  //初回ループまたは1.5[h]ごとに天気情報を更新
  if (counter % 5400 == 0) {
    updateWeather();//Webから天気を取得
    drawWeather();//天気を描画
  }

  counter++;//符号なしなのでオーバーフローしてもok
  delay(1000);//1[s]

  if (digitalRead(M5_BUTTON_HOME) == LOW) {
        // HOMEボタンを押されたときの処理
        M5.Lcd.fillScreen(0x0000);
        M5.Lcd.drawBitmap(8, 12, 64, 64, w_icon[MAIL]);//メールアイコンを描画
        M5.Lcd.drawString("Sending Now", 4, 0);//文字列を描画
        googleHomeConnection( "ja", "もうすぐご飯です。そろそろ作業を終わりにしてください。" );
        M5.Lcd.fillScreen(0x0000);
        drawWeather();//天気を描画
        while (digitalRead(M5_BUTTON_HOME) == LOW) ;
    }
    if (digitalRead(M5_BUTTON_RST) == LOW) {
        // RSTボタンを押されたときの処理
        M5.Lcd.fillScreen(0x0000);
        M5.Lcd.drawBitmap(8, 12, 64, 64, w_icon[MAIL]);//メールアイコンを描画
        M5.Lcd.drawString("Sending Now", 4, 0);//文字列を描画
        googleHomeConnection( "ja", "何か用事があるみたいですよ、呼ばれてます。" );
        M5.Lcd.fillScreen(0x0000);
        drawWeather();//天気を描画
        while (digitalRead(M5_BUTTON_RST) == LOW) ;
    }
}
