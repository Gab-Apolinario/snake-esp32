// ───── BIBLIOTECAS ─────────────
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ───── PINOS DISPLAY ─────────────
//#define WOKWI_SIM //ATIVAR quando estiver simulando virtualmente (Wokwi)

#define TFT_CS    D7
#define TFT_RST   D9
#define TFT_DC    D4
Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ───── CORES (display BGR) ───────────────────
//inverte valores RGB
#define VERMELHO         ST77XX_BLUE
#define AZUL             ST77XX_RED
#define VERDE            ST77XX_GREEN
#define BRANCO           ST77XX_WHITE
#define PRETO            ST77XX_BLACK
#define CINZA            0x4208
#define VERDE_ESCURO     0x0400

// ───── LIMITES REAIS DO DISPLAY ──────────────
// Testado para não cortar pixels/imagens
#define TELA_X_MIN   4
#define TELA_X_MAX   125
#define TELA_Y_MIN   2
#define TELA_Y_MAX   125

// ───── GRID DO JOGO ──────────────────────────
// Área de jogo: y=12 até y=125 (113px → 14 linhas de 8px = 112px)
// Score bar:    y=2  até y=11  (10px, textSize 1 = 8px)
// X:            x=4  até x=124 (120px → 15 colunas de 8px)
#define CEL        8                   //tamanho de cada célula em pixels
#define COLUNAS    16                  //quantas células cabem na horizontal
#define LINHAS     14                  //quantas células cabem na vertical
#define AREA_X     TELA_X_MIN          // 4
#define AREA_Y     12                  // abaixo da barra de score
#define MAX_COBRA  (COLUNAS * LINHAS)  // 210 — cobra pode preencher tudo
#define TELA_LARGURA 122
#define TELA_ALTURA  124

// ───── DIREÇÕES (ENUM!!) ──────────────────────────────
#define CIMA      0
#define DIREITA   1
#define BAIXO     2
#define ESQUERDA  3

// ───── PINOS JOYSTICK ─────────────────────────────────
#define PINO_LED  D6
#define PINO_VRX  A1 
#define PINO_VRY  A2

// ───── ESTADO DO JOGO / VARIAVEIS ────────────────────────
struct Pos { int x, y; }; //guarda posição em grid

Pos   cobra[MAX_COBRA]; //array de 210 posições
                        //[0] É SEMPRE A CABEÇA
                        //a cobra se move, faz SHIFT dos elementos
                        //cada seguimento herda o position do seguimento da frente
int   tamCobra;
int   direcao;
int   proximaDirecao; // buffer de input — evita virar duas vezes num tick

Pos   comida;

int   pontos;
int   pontosVitoria; //Nº de pontos para alcançar a vitória

bool gameOver;

int velocidadeJogo; //Velocidade INICIAL do jogo. (Nº < é mais rápido)
float aumentoVelRate; //multiplicador da velocidade a cada vez que come a fruta vermelha

unsigned long ultimoTick = 0;

bool frutaBuff;
int porcentagemBuff = 20; //PORCENTAGEM DE SER FRUTA COM BUFF (0 - 100)
unsigned long inicioBuff = 0;
bool buffAtivo = false;
int velocidadeAntesBuff;
int tempoBuff = 3000; //DURAÇÃO DO BUFF

#define ESTADO_INICIO     0
#define ESTADO_JOGANDO    1
#define ESTADO_GAMEOVER   2
#define ESTADO_VITORIA    3

int estadoJogo = ESTADO_INICIO;

// ───── ANIMAÇÃO COBRA TELA DE INICIO ────────────────────────

//rota que contorna a tela de início
Pos rota[] = {
  {0, 0},   // canto superior esquerdo
  {15, 0},  // vai para direita
  {15, 13}, // desce
  {0, 13},  // vai para esquerda
  {0, 0}    // sobe — volta ao início
};

int numWaypoints = 5;

//estado da animação
Pos   animCobra[20];   // corpo da cobra animada
int   animTam = 5;     // tamanho da cobra decorativa
int   animWaypoint;    // qual waypoint está mirando agora
Pos   animHead;        // posição atual da cabeça

// ═══════════════════════════════════════════
// FUNÇÕES DE DESENHO
// ═══════════════════════════════════════════

//centraliza texto horizontalmente
//Y para controlar a altura manualmente
void printCentrado(const char* texto, int y, uint16_t cor, int tamanho, uint16_t corFundo) {
  int larguraTexto = strlen(texto) * 6 * tamanho;
  int x = TELA_X_MIN + (TELA_LARGURA - larguraTexto) / 2;
  display.setTextSize(tamanho);
  display.setTextColor(cor, corFundo);
  display.setCursor(x, y);
  display.print(texto);
}

// converte célula do grid em pixel
int celaParaPixelX(int col) { return AREA_X + col * CEL; }
int celaParaPixelY(int row) { return AREA_Y + row * CEL; }

void desenharCelula(int col, int row, uint16_t cor) {
  // -1 cria 1px de espaço entre células — visual de grade
  display.fillRect(celaParaPixelX(col), celaParaPixelY(row), CEL - 1, CEL - 1, cor);
}

void desenharScore() {
  display.setTextSize(1);
  display.setTextColor(BRANCO, PRETO);
  display.setCursor(15, TELA_Y_MIN + 1);
  char buf[24];
  sprintf(buf, "SNAKE  SCORE: %d/%d ", pontos, pontosVitoria);
  display.print(buf);
  display.drawFastHLine(TELA_X_MIN, 11, 121, CINZA);  //linha separadora
}

void gerarComida(){
  //VALIDAÇÃO POSIÇÃO
  bool posValida;

  int sorteioFruta = random(0, 100);
  frutaBuff = sorteioFruta < porcentagemBuff;

  if (frutaBuff) {
    do
    {
      posValida = true;
      comida.x = random(0, COLUNAS);
      comida.y = random(0, LINHAS);
      for (int i = 0; i < tamCobra; i++)
      {
        if (cobra[i].x == comida.x && cobra[i].y == comida.y)
        {
          posValida = false;
          break;
        }
      }
    } while (!posValida);
    
    desenharCelula(comida.x, comida.y, AZUL);
  }else{
    do
    {
      posValida = true;
      comida.x = random(0, COLUNAS);
      comida.y = random(0, LINHAS);
      for (int i = 0; i < tamCobra; i++)
      {
        if (cobra[i].x == comida.x && cobra[i].y == comida.y)
        {
          posValida = false;
          break;
        }
      }
    } while (!posValida);
    desenharCelula(comida.x, comida.y, VERMELHO);
  }
}

void tickAnimacao() {
  Pos alvo = rota[animWaypoint];

  //move a cabeça um passo em direção ao alvo
  if      (animHead.x < alvo.x) animHead.x++;
  else if (animHead.x > alvo.x) animHead.x--;
  else if (animHead.y < alvo.y) animHead.y++;
  else if (animHead.y > alvo.y) animHead.y--;

  //chegou no waypoint? avança para o próximo
  if (animHead.x == alvo.x && animHead.y == alvo.y) {
    animWaypoint = (animWaypoint + 1) % numWaypoints; // % = loop circular
    if(animWaypoint == 0 || animWaypoint == 3){
      animTam++;
    }
  }

  //apaga o rabo
  desenharCelula(animCobra[animTam-1].x, animCobra[animTam-1].y, PRETO);

  //shift do corpo
  for (int i = animTam-1; i > 0; i--)
    animCobra[i] = animCobra[i-1];
  animCobra[0] = animHead;

  //desenha
  desenharCelula(animCobra[0].x, animCobra[0].y, VERDE);
  for (int i = 1; i < animTam; i++)
    desenharCelula(animCobra[i].x, animCobra[i].y, VERDE_ESCURO);
}

//EFEITO BUFF
void buffFruta(){
  if (!buffAtivo){ //garante que só grava a velocidade se o buff não estiver ativado
    velocidadeAntesBuff = velocidadeJogo;
  }
  velocidadeJogo = 600;
  buffAtivo = true;
  inicioBuff = millis();
}

// ═══════════════════════════════════════════
// INICIAR JOGO
// ═══════════════════════════════════════════
void iniciarJogo() {
  display.fillScreen(PRETO);     //LIMPA TUDO NA TELA
  tamCobra       = 3;            //tamanho inicial da cobra
  direcao        = DIREITA;      //direção inicial da cobra
  proximaDirecao = DIREITA;      //buffer de input zerado
  pontos         = 0;
  gameOver = false;
  velocidadeJogo = 400;
  aumentoVelRate = 0.97;
  pontosVitoria = 30;


  //Pos inicial da cobra
  cobra[0] = {4, 7}; //cabeça
  cobra[1] = {3, 7}; //corpo
  cobra[2] = {2, 7}; //rabo

  for (int i = 0; i < tamCobra; i++)
    desenharCelula(cobra[i].x, cobra[i].y, VERDE);

  desenharScore();
  gerarComida();
}

// ═══════════════════════════════════════════
// INPUT — lê joystick e bufferiza direção
// para não inverter direção (não pode ir p/ trás)
// ═══════════════════════════════════════════
void lerJoystick() {
  int y = analogRead(PINO_VRX);
  int x = analogRead(PINO_VRY);

  if (x > 3000 && direcao != DIREITA){
    proximaDirecao = ESQUERDA;
  }
  if (x < 1000 && direcao != ESQUERDA){
    proximaDirecao = DIREITA;
  }
  if (y > 3000 && direcao != CIMA){
    proximaDirecao = BAIXO;
  }
  if (y < 1000 && direcao != BAIXO){
    proximaDirecao = CIMA;
  }
}

// ═══════════════════════════════════════════
// TICK DO JOGO — roda uma vez por loop
// ═══════════════════════════════════════════
void tickJogo() {
  //aplica direção bufferizada
  direcao = proximaDirecao;

  //calcula nova cabeça
  Pos novaHead = cobra[0];
  if (direcao == CIMA){
    novaHead.y--;
  }

  if (direcao == BAIXO){
    novaHead.y++;
  }

  if (direcao == DIREITA){
    novaHead.x++;
  }

  if (direcao == ESQUERDA){
    novaHead.x--;
  }

  //colisão com borda do grid
  if (novaHead.x < 0 || novaHead.x >= COLUNAS ||
      novaHead.y < 0 || novaHead.y >= LINHAS) {
    gameOver = true;
    return;
  }

  //colisão com o próprio corpo (ignora rabo — ele vai se mover)
  for (int i = 0; i < tamCobra - 1; i++) {
    if (cobra[i].x == novaHead.x && cobra[i].y == novaHead.y) {
    gameOver = true;
    return;
    }
  }

  // comeu?
  bool comeu = (novaHead.x == comida.x && novaHead.y == comida.y);

  if(comeu && frutaBuff){
    //cresce
    if (tamCobra < MAX_COBRA){
      tamCobra++;
      buffFruta();
    }
  }else if (comeu && !frutaBuff) { //fruta normal
    if (tamCobra < MAX_COBRA){
      tamCobra++;
      pontos++;
      desenharScore();
      
      //LED pisca
      digitalWrite(PINO_LED, HIGH);
      delay(120);
      digitalWrite(PINO_LED, LOW);
    }
  }else{
    //apaga o rabo
    desenharCelula(cobra[tamCobra - 1].x, cobra[tamCobra - 1].y, PRETO);
  }

  //shift do corpo
  for (int i = tamCobra - 1; i > 0; i--)
    cobra[i] = cobra[i - 1];
  cobra[0] = novaHead;

  //desenha nova cabeça em cor ligeiramente diferente para destacar
  display.fillRect(celaParaPixelX(cobra[0].x), celaParaPixelY(cobra[0].y),
               CEL - 1, CEL - 1, VERDE);
  //escurece o segmento anterior (era cabeça, vira corpo)
  if (tamCobra > 1)
    display.fillRect(celaParaPixelX(cobra[1].x), celaParaPixelY(cobra[1].y),
                 CEL - 1, CEL - 1, VERDE_ESCURO); 

  if (comeu){
    gerarComida();
  }
}

// ═══════════════════════════════════════════
// TELAS
// ═══════════════════════════════════════════
void telaInicio() {
  display.fillScreen(PRETO);

  estadoJogo = ESTADO_INICIO;

  display.setTextSize(3);
  display.setTextColor(VERDE, PRETO);
  display.setCursor(25, 40);
  display.print("SNAKE");

  display.setTextSize(1);
  display.setTextColor(BRANCO);
  display.setCursor(18, 85);
  display.print("Mova para iniciar");
}

void telaGameOver() {
  // overlay preenche área de jogo
  display.fillRect(AREA_X, AREA_Y, 127, 112, PRETO);

  estadoJogo = ESTADO_GAMEOVER;

  display.setTextSize(2);
  display.setTextColor(VERMELHO);
  printCentrado("GAME", 30, VERMELHO, 2, PRETO);
  printCentrado("OVER", 52, VERMELHO, 2, PRETO);

  display.setTextSize(1);
  display.setTextColor(VERDE);
  display.setCursor(40, 80);
  char buf[20];
  sprintf(buf, "Pontos: %d", pontos);
  display.print(buf);

  display.setTextColor(BRANCO);
  display.setCursor(18, 100);
  display.print("Mova p/ reiniciar");
}

void telaVitoria(){
  display.fillScreen(VERDE_ESCURO);

  estadoJogo = ESTADO_VITORIA;

  printCentrado("VITORIA", 40, BRANCO, 2, VERDE_ESCURO);

  display.setTextSize(1);
  display.setTextColor(BRANCO);
  display.setCursor(40, 80);
  char buf[20];
  sprintf(buf, "Pontos: %d", pontos);
  display.print(buf);

  display.setTextColor(BRANCO);
  display.setCursor(18, 100);
  display.print("Mova p/ reiniciar");
}


// ═══════════════════════════════════════════
// SETUP e LOOP
// ═══════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);

  display.initR(INITR_BLACKTAB);

//SIMULADOR WOKWI
#ifdef WOKWI_SIM
  display.setRotation(0);
  // Corrige o espelhamento horizontal no simulador
  // Sobrescreve o MADCTL com flip no eixo X
  uint8_t madctl = 0xE0;  // rotação 3 + espelha colunas
  display.sendCommand(ST77XX_MADCTL, &madctl, 1);
#else
  display.setRotation(1);
#endif

  randomSeed(analogRead(A0));

  telaInicio();
}

void loop() {

  lerJoystick();

  if (estadoJogo == ESTADO_INICIO) {
    int x = analogRead(PINO_VRX);
    int y = analogRead(PINO_VRY);
    if (x > 3000 || x < 1000 || y > 3000 || y < 1000) {
      delay(300);
      iniciarJogo();
      estadoJogo = ESTADO_JOGANDO;
      ultimoTick = millis();
      return;
    }
    tickAnimacao();
    delay(200);
    return;
  }

  if (estadoJogo == ESTADO_GAMEOVER || estadoJogo == ESTADO_VITORIA) {
    int x = analogRead(PINO_VRX);
    int y = analogRead(PINO_VRY);
    if (x > 3000 || x < 1000 || y > 3000 || y < 1000) {
      delay(300);
      iniciarJogo();
      estadoJogo = ESTADO_JOGANDO;
      ultimoTick = millis();
    }
    delay(100);
    return;
  }

  if (millis() - ultimoTick >= (unsigned long)velocidadeJogo) {
    ultimoTick = millis();

    if(buffAtivo && millis() - inicioBuff >= tempoBuff){
      velocidadeJogo = velocidadeAntesBuff;
      buffAtivo = false;
    }
    tickJogo();

    if (gameOver) {
      telaGameOver();
    } else if (pontos >= pontosVitoria) {
      telaVitoria();
    }
  }
}