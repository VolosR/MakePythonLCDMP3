
#include <TFT_eSPI.h> 
#include "Arduino.h"
#include "Audio.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "fonts.h"

TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite img = TFT_eSprite(&tft);


//SD Card
#define SD_CS 22
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

//Digital I/O used  //Makerfabs Audio V2.0
#define I2S_DOUT 27
#define I2S_BCLK 26
#define I2S_LRC 25

const int Pin_vol_up = 39;
const int Pin_vol_down = 36;
const int Pin_mute = 35;

const int Pin_previous = 15;
const int Pin_pause = 33;
const int Pin_next = 2;

int visuals[6]={5,20,14,26,8,14};
int dir[5];

Audio audio;

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

bool paused=0;

struct Music_info
{
    String name;
    int length;
    int runtime;
    int volume;
    int status;
    int mute_volume;
    int m;
    int s;
} music_info = {"", 0, 0, 0, 0, 0,0,0};

String file_list[20];
String runtimes[20];
int file_num = 0;
int file_index = 0;
bool started=0;
int counter=0;

void setup()
{
    //IO mode init
    pinMode(Pin_vol_up, INPUT_PULLUP);
    pinMode(Pin_vol_down, INPUT_PULLUP);
    pinMode(Pin_mute, INPUT_PULLUP);
     pinMode(Pin_previous,INPUT);
    pinMode(Pin_pause, INPUT_PULLUP);
    pinMode(Pin_next, INPUT_PULLUP);

    Serial.begin(115200);
    tft.init();
    tft.setRotation(0);
    tft.setSwapBytes(true);
    

    
    ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
    ledcAttachPin(5, pwmLedChannelTFT);
    ledcWrite(pwmLedChannelTFT, 144);

    //SD(SPI)
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000);
    if (!SD.begin(SD_CS, SPI))
    {
        Serial.println("Card Mount Failed");
        lcd_text("SD ERR");
        while (1);
    }
    else
    {
        lcd_text("SD OK");
    }

    //Read SD
    file_num = get_music_list(SD, "/", 0, file_list);
    Serial.print("Music file count:");
    Serial.println(file_num);
    Serial.println("All music:");
    int y=0;
    tft.setTextColor(0xBDD7,TFT_BLACK);
    for (int i = 0; i < file_num; i++)
    {
        Serial.println(file_list[i]);
        tft.drawString(file_list[i].substring(1,file_list[i].length()-4),18,118+y,2);
        y=y+15;
    }

    //Audio(I2S)
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // 0...21
    drawStart();
  
    open_new_song(file_list[file_index]);
    print_song_time();
    drawVolume();
}

uint run_time = 0;
uint button_time = 0;

int tempSeconds=0;
int tempMinutes=0;

bool dd[6]={1,1,0,1,0,1};
void loop()
{
    audio.loop();

    if(paused==0)
    for(int i=0;i<5;i++)
      {
    if(dd[i]==1)
    visuals[i]=visuals[i]+1;
    if(dd[i]==0)
    visuals[i]=visuals[i]-1;

    if(visuals[i]==0 || visuals[i]==30)
    dd[i]=!dd[i];
    
    tft.fillRect(110+(i*6),4,3,4+visuals[i],TFT_BLACK);
    tft.fillRect(110+(i*6),4+visuals[i],3,34-visuals[i],TFT_ORANGE);
      }

    if (millis() - run_time > 1000)
    {
        run_time = millis();
        print_song_time();
        tft_music();
    }

    if (millis() - button_time > 300)
    {
        
        if (digitalRead(Pin_next) == 0)
        {
            Serial.println("Pin_next");
            if (file_index < file_num - 1)
                file_index++;
            else
                file_index = 0;
            drawStart();    
            open_new_song(file_list[file_index]);
            print_song_time();
            button_time = millis();
        }
        if (digitalRead(Pin_pause) == 0)
        {

            Serial.println("Pin_previous");
            if (file_index > 0)
                file_index--;
            else
                file_index = file_num - 1;
              
            drawStart();
            open_new_song(file_list[file_index]);
            button_time = millis();
        }
        if (digitalRead(Pin_vol_up) == 0)
        {
            Serial.println("Pin_vol_up");
            if (music_info.volume < 21)
                music_info.volume++;
            audio.setVolume(music_info.volume);
            drawVolume();
            button_time = millis();
        }
        if (digitalRead(Pin_vol_down) == 0)
        {
            Serial.println("Pin_vol_down");
            if (music_info.volume > 0)
                music_info.volume--;
            audio.setVolume(music_info.volume);
            drawVolume();
            button_time = millis();
        }
        
        
        
        if (digitalRead(Pin_mute) == 0)
        {
            Serial.println("Pin_pause");
            audio.pauseResume();
            paused=!paused;
            button_time = millis();
        }
    }

    
    if (Serial.available())
    {
        String r = Serial.readString();
        r.trim();
        if (r.length() > 5)
        {
            audio.stopSong();
            open_new_song(file_list[0]);
            print_song_info();
        }
        else
        {
            audio.setVolume(r.toInt());
        }
    }
}

void open_new_song(String filename)
{

    music_info.name = filename.substring(1, filename.indexOf("."));
    audio.connecttoFS(SD, filename.c_str());
    music_info.runtime = audio.getAudioCurrentTime();
    music_info.length = audio.getAudioFileDuration();
    music_info.volume = audio.getVolume();
    music_info.status = 1;
    music_info.m=music_info.length/60;
    music_info.s=music_info.length%60;  
}

void tft_music()
{
    int line_step = 28;
    int line = map(music_info.runtime,0,music_info.length,10,210);
    char buff[20];

    tempSeconds=music_info.runtime%60;
    tempMinutes=music_info.runtime/60;

    tft.fillRect(0,87,240,9,TFT_BLACK);
    tft.drawLine(10,92,210,92,0xBDD7);
    tft.drawLine(10,90,10,94,0xBDD7);
    tft.drawLine(210,90,210,94,0xBDD7);
    tft.fillRect(line,88,6,8,TFT_GREEN);

    String ts,tm;

    if(tempSeconds<10)
    ts="0"+String(tempSeconds);
    else
    ts=String(tempSeconds);

    if(tempMinutes<10)
    tm="0"+String(tempMinutes);
    else
    tm=String(tempMinutes);
    
    tft.setFreeFont(&FreeMono9pt7b);
    tft.setTextColor(0xFC3D,TFT_BLACK); 
  
    tft.drawString(music_info.name,4,66);
    tft.drawString(String(music_info.m)+":"+String(music_info.s),190,100);

    tft.setTextColor(0xBDD7,TFT_BLACK);
    tft.setFreeFont(&DSEG7_Classic_Regular_20);
    tft.drawString(tm+":"+ts,5,20);
}

void drawVolume()
{
int v=map(music_info.volume,0,21,0,8);
tft.fillRect(180,18,54,24,TFT_BLACK);

for(int i=0;i<v;i++)
{
  tft.drawLine(198+i*4, 40, 198+i*4, 37-(i*2),0xBDD7);
  }
  
  }

void drawStart(void)
{
    
    tft.setTextColor(TFT_GREEN,TFT_BLACK);
    tft.drawString("TIME",6,0,2);
    tft.drawString("VOLUME",182,0,2);
    tft.drawString("PLAYING",6,45,2);
    tft.setTextColor(TFT_ORANGE,TFT_BLACK);
    tft.drawString("PLAYLIST",6,100,2);
    tft.fillRect(4,64,232,22,TFT_BLACK);
    tft.fillRect(0,112,14,130,TFT_BLACK); 
    tft.fillCircle(7,125+(file_index*15),3,TFT_ORANGE);
}

void lcd_text(String text)
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE); 
    tft.setCursor(0, 0);            
    tft.println(text);
    tft.fillScreen(TFT_BLACK);
}

void print_song_info()
{
    Serial.println("***********************************");
    Serial.println(audio.getFileSize());
    Serial.println(audio.getFilePos());
    Serial.println(audio.getSampleRate());
    Serial.println(audio.getBitsPerSample());
    Serial.println(audio.getChannels());
    Serial.println(audio.getAudioFileDuration());
    Serial.println("***********************************");
}

void print_song_time()
{
    
    music_info.runtime = audio.getAudioCurrentTime();
    music_info.length = audio.getAudioFileDuration();
    music_info.volume = audio.getVolume();
    music_info.m=music_info.length/60;
    music_info.s=music_info.length%60;
}

int get_music_list(fs::FS &fs, const char *dirname, uint8_t levels, String wavlist[30])
{
    Serial.printf("Listing directory: %s\n", dirname);
    int i = 0;
    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return i;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return i;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
        }
        else
        {
            String temp = file.name();
            if (temp.endsWith(".wav"))
            {
                wavlist[i] = temp;
                i++;
            }
            else if (temp.endsWith(".mp3"))
            {
                wavlist[i] = temp;
                i++;
            }
        }
        file = root.openNextFile();
    }
    return i;
}


void audio_info(const char *info)
{
    Serial.print("info        ");
    Serial.println(info);
}
void audio_id3data(const char *info)
{ //id3 metadata
    Serial.print("id3data     ");
    Serial.println(info);
}

void audio_eof_mp3(const char *info)
{ //end of file
    Serial.print("eof_mp3     ");
    Serial.println(info);
    file_index++;
    if (file_index >= file_num)
    {
        file_index = 0;
    }
    open_new_song(file_list[file_index]);
}
void audio_showstation(const char *info)
{
    Serial.print("station     ");
    Serial.println(info);
}
void audio_showstreaminfo(const char *info)
{
    Serial.print("streaminfo  ");
    Serial.println(info);
}
void audio_showstreamtitle(const char *info)
{
    Serial.print("streamtitle ");
    Serial.println(info);
}
void audio_bitrate(const char *info)
{
    Serial.print("bitrate     ");
    Serial.println(info);
}
void audio_commercial(const char *info)
{ //duration in sec
    Serial.print("commercial  ");
    Serial.println(info);
}
void audio_icyurl(const char *info)
{ //homepage
    Serial.print("icyurl      ");
    Serial.println(info);
}
void audio_lasthost(const char *info)
{ //stream URL played
    Serial.print("lasthost    ");
    Serial.println(info);
}
void audio_eof_speech(const char *info)
{
    Serial.print("eof_speech  ");
    Serial.println(info);
}
