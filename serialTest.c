#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#include <lcd.h>
#include <wiringPi.h>
#include <wiringSerial.h>

//USE WIRINGPI PIN NUMBERS
#define LCD_RS  13               //Register select pin
#define LCD_E   18               //Enable Pin
#define LCD_D4  21               //Data pin 4
#define LCD_D5  24               //Data pin 5
#define LCD_D6  26               //Data pin 6
#define LCD_D7  27               //Data pin 7
#define BUTTON_DOWN  25          // PA07
#define BUTTON_UP  23         // PA10
#define BUTTON_ENTER  19         // PA20

#define SIZE_MENU_1 34
#define SIZE_MENU_2 9
#define TURN_BACK (SIZE_MENU_2 - 1)
#define EXIT      availableUnits
#define TWO_SECONDS 2000
#define ANALOG_PIN index == 1
#define MONITORING_ANALOG index == 2
#define MONITORING_ALL (index == (SIZE_MENU_2 - 2))

#define UART_3 "/dev/ttyS3"
#define BAUD_RATE 9600

#define CONSULT     index % 2
#define MONITORING !(CONSULT)
#define MAX_UNITS 32


void sendData(int fd, unsigned char* array, unsigned char pos) {
    serialPutchar(fd, array[pos]);
    delay(2);
}

// Implementa o timeout
int recvDigitalData(int fd) {
    if (serialDataAvail(fd) > 0) {
        printf("CONSEGUIU LER\n");
        return serialGetchar(fd); 
    }
    delay(8);
    if (serialDataAvail(fd) > 0) {
        printf("CONSEGUIU LER\n");
        return serialGetchar(fd); 
    }
    printf("NAO CONSEGUIU LER\n");
    return -1;
}

int reachUnit(int fd, char *str, unsigned char *select, unsigned char *deselect, int unitId) {
    int recvData;
    sendData(fd, select, unitId);
    recvData = recvDigitalData(fd);
    if(recvData > -1) {
        sprintf(str, "Select Unit %d", unitId + 1);
        printf("REACH UNIT\n");
        return 1;
    }
    sendData(fd, deselect, unitId);
    return 0;
}

int bytes2int(int fd) {
    if(serialDataAvail(fd) > 0) {
        int quocient = serialGetchar(fd);
        delay(2);
        int remainder = serialGetchar(fd);
        return (quocient * 10) + remainder;
    }
    return -1;
}

int recvAnalogData(int fd) {
    int analogData = bytes2int(fd);
    if(analogData > -1) return analogData;
    delay(8);
    analogData = bytes2int(fd);
    if(analogData > -1) return analogData;
    return analogData; // -1
}


void refreshPos(int* pos, signed short size) {
    if (*pos < 0) *pos = size - 1;
    else if (*pos == size) *pos = 0;
}

void lcdddPuts(int lcdfd, char str[], int time) {
    lcdClear(lcdfd);
    lcdPuts(lcdfd, str);
    delay(time);
}


int main() {
    
    int buttonDownState;
    int buttonUpState;
    int buttonEnterState;

    int idxMonitoring;
    int lcdfd;
    int uartfd;
    // Guarda o indice da node selecionada
    int selectedNode;
    // Menu ativo no momento
    int meun1Active = TRUE;
    int menu2Active = FALSE;

    int availableUnits = 0;

    int choiceMenu1;
    int choiceMenu2;

    // Guarda os dados provenientes da Node
    int recvData = 0;

    // Variavel para controlar o que vai ser mostrado no Menu
    int index = 0;

    //unsigned char analogBytes[2];
    //unsigned char followCommands[] = { 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6 };
    char monitoringLabels[3][10] = { {"D0"}, {"D1"}, {"A0"} };
    char monitoringLabels2[3][10] = { {"A0"}, {"D0"}, {"D1"} };
    unsigned char monitoringArray[] = { 0xC3, 0xC5, 0xC1 };
    unsigned char consultCommands[] = { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7 };
    unsigned char selectNode[MAX_UNITS] = {
                                        0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                        0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF ,0x10, 0x11,
                                        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
                                        0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
                                    };
    unsigned char deselectNode[MAX_UNITS] = {   
                                        0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
                                        0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
                                        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
                                        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0
                                    };
    // Menus
    char vetor_menu01[SIZE_MENU_1][30];
    char vetor_menu02[SIZE_MENU_2][30] = {
                                            {"Turn on LED"},
                                            {"Query A0"},
                                            {"Monitoring A0"},
                                            {"Query D0"},
                                            {"Monitoring D0"},
                                            {"Query D1"},
                                            {"Monitoring D1"},
                                            {"Monitoring all of them"},
                                            {"Turn Back"}
                                        };

    // Inicializar biblioteca
    if (wiringPiSetup () == -1)
    {
        fprintf (stdout, "Unable to start wiringPi: %s\n", strerror (errno)) ;
        return 1;
    }

    // Setar botões como entrada
    pinMode(BUTTON_DOWN, INPUT);
    pinMode(BUTTON_UP, INPUT);
    pinMode(BUTTON_ENTER, INPUT);
    

    // Configurar UART
    if ((uartfd = serialOpen (UART_3, BAUD_RATE)) < 0){
        fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno)) ;
        return 1;
    }

    // Iniciar LCD
    if ((lcdfd = lcdInit (2, 16, 4, LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7, 0, 0, 0, 0) > 0))
    {
        printf("Unable to init LCD");
        return 1;
    }

    //lcdClear(lcdfd);

    // DESCOBRINDO UNIDADES ONLINES
    for(int i = 0; i < MAX_UNITS; i++) {
        recvData = reachUnit(uartfd, vetor_menu01[i], selectNode, deselectNode, i);
        if(recvData > 0) {
            availableUnits++;
        }else{
            selectNode[i] = -1;
            deselectNode[i] = -1;
            vetor_menu01[i][0] = '\0';
        }
    }
    int cont = 0;
    int cont2 = 0;
    int cont3 = 0;
    for (int i = 0; i < MAX_UNITS; i++){
        if(selectNode[i] < 255){
            selectNode[cont] = selectNode[i];
            cont++;
        }
        if(deselectNode[i] < 255){
            deselectNode[cont2] = deselectNode[i];
            cont2++;
        }
        if(vetor_menu01[i][0] != '\0'){
            strcpy(vetor_menu01[cont3], vetor_menu01[i]);
            cont3++;
        }
    }


    sprintf(vetor_menu01[availableUnits], "Monitor All");
    availableUnits++;
    sprintf(vetor_menu01[availableUnits], "Sair");
   
    // sprintf(vetor_menu01[availableUnits], "Exit");

    // MOSTRAR NODES CONECTADAS
    // for(int i=0; i < availableUnits; i++) printf("[%d] -> %s\n", i, vetor_menu01[i]);
    // printf("availableUnits -> %d\n", availableUnits);

    // for(int i=0; i < availableUnits; i++) printf("[%d] -> %u\n", i, consultCommands[i]);
    // return 0;

    lcdddPuts(lcdfd, vetor_menu01[index], 0);
    while(TRUE){
        // Verificar se algum botão foi pressionado
        buttonDownState = digitalRead(BUTTON_DOWN);
        buttonUpState = digitalRead(BUTTON_UP);
        buttonEnterState = digitalRead(BUTTON_ENTER);
        
        // Entender esse Dalay, se precisa ou não
        delay(200);
        
        if (!buttonDownState) {
            // O botão foi pressionado
            index++;
            
            // Verificar se o que vai ser mostrado é o menu 1 ou 2
            if (meun1Active) {
                // Se posição passar da quantidade de itens do menu, levar para a posição zero do menu
                refreshPos(&index, availableUnits + 1);
                // lcdClear(lcdfd);
                // lcdPrintf(lcdfd, "%s", vetor_menu01[index]);
                lcdddPuts(lcdfd, vetor_menu01[index], 0);
            
            }else if (menu2Active) {
                // Se posição passar da quantidade de itens do menu, levar para a posição zero do menu
                refreshPos(&index, SIZE_MENU_2);
                //mostrar_menu_02(vetor_menu02, index);
                // lcdClear(lcdfd);
                // lcdPrintf(lcdfd, "%s", vetor_menu02[index]);
                lcdddPuts(lcdfd, vetor_menu02[index], 0);
            }
        
        }
        else if (!buttonUpState) {
            // O botão foi pressionado
            index--;

            // Verificar se o que vai ser mostrado é o menu 1 ou 2
            if (meun1Active) {
                // Se posição for menor que a quantidade de itens do menu, levar para a posição 32 do menu
                // if (index == -1){
                //     index = qtdItensMenu01 - 1;
                // }
                // mostrar_menu_01(vetor_menu01, index);
                refreshPos(&index, availableUnits + 1);
                // lcdClear(lcdfd);
                // lcdPrintf(lcdfd, "%s", vetor_menu01[index]);
                lcdddPuts(lcdfd, vetor_menu01[index], 0);
            
            }else if (menu2Active) {
                // Se posição for menor que a quantidade de itens do menu, levar para a posição 6 do menu
                // if (index == -1){
                //     index = qtdItensMenu02 - 1;
                // }
                refreshPos(&index, SIZE_MENU_2);
                //mostrar_menu_02(vetor_menu02, index);
                // lcdClear(lcdfd);
                // lcdPrintf(lcdfd, "%s", vetor_menu02[index]);
                lcdddPuts(lcdfd, vetor_menu02[index], 0);
            }
        
        }
        else if (!buttonEnterState) {
            // O botão foi pressionado
            if (meun1Active) {
                // Verificar se apertou enter na posição Sair
                choiceMenu1 = index;
                if (choiceMenu1 == EXIT) {
                    lcdddPuts(lcdfd, ":[", 0);
                    break;

                }else if (choiceMenu1 == availableUnits - 1){ // Monitorar tudo em todas as nodes
                    printf("VOCE DECIDIU MONITORAR TODAS AS UNIDADES\n");
                    while(digitalRead(BUTTON_DOWN)) {
                        idxMonitoring = 0;
                        for (int i = 0; i < availableUnits - 1; i++){
                            lcdddPuts(lcdfd, "Selecting Unit...", TWO_SECONDS);
                            sendData(uartfd, selectNode, i);
                            recvData = recvDigitalData(uartfd);
                            lcdddPuts(lcdfd, "NODE SELECTED", TWO_SECONDS);
                            // Verificar estado dos Pinos
                            idxMonitoring = 0;
                            while(digitalRead(BUTTON_DOWN)) {
                                serialFlush(uartfd);
                                //recvData = recvDigitalData(uartfd);
                                lcdClear(lcdfd);
                                sendData(uartfd, monitoringArray, idxMonitoring);
                                if(idxMonitoring == 2) recvData = recvAnalogData(uartfd);
                                else recvData = recvDigitalData(uartfd);
                                printf("IDX -> %d\n", idxMonitoring);
                                lcdPrintf(lcdfd, "Value %s: %d", monitoringLabels[idxMonitoring], recvData);
                                lcdPosition(lcdfd, 0, 1);
                                lcdPuts(lcdfd, "<ENTER TO EXIT>");
                                delay(1000);
                                if (idxMonitoring == 2) break; 
                                idxMonitoring++;
                            }
                            // Tirar seleção da node
                            //lcdddPuts(lcdfd, "Deselecting the unit...", TWO_SECONDS);
                            sendData(uartfd, deselectNode, i);
                            recvData = recvDigitalData(uartfd);
                            printf("DESELECT RECV DATA -> %d\n", recvData);
                        }
                    }
                    //  lcdClear(lcdfd);
                    //  lcdPuts(lcdfd, "SAINDO DO MONITORAMENTO");
                    //  delay(1000);
                    while(!digitalRead(BUTTON_DOWN));
                    lcdddPuts(lcdfd, vetor_menu01[index], 0);
                    continue;
                }
                else {
                    lcdddPuts(lcdfd, "Selecting Unit...", TWO_SECONDS);
                    // Seleciona a Node com o ID escolhido
                    sendData(uartfd, selectNode, index);
                    // Salva a index da node selecionada no vetor de ID das Nodes
                    recvData = recvDigitalData(uartfd);
                    // lcdClear(lcdfd);
                    if(recvData > -1) {
                        printf("SELECT RECV DATA -> %d\n", recvData);
                        selectedNode = index;
                        //serialPutchar(fd, select_node[index - 1]);
                        // Desabilita menu 1
                        meun1Active = FALSE;
                        // Habilita menu 2
                        menu2Active = TRUE;
                        // Reseta o contador de posicoes
                        index = 0;
                        // lcdPuts(lcdfd, "UNIDADE SELECIONADA COM SUCESSO");
                        // delay(3000);
                        lcdddPuts(lcdfd, "Success on selecting the unit", TWO_SECONDS);
                        // lcdClear(lcdfd);
                        // lcdPrintf(lcdfd, "%s", vetor_menu02[index]);
                        lcdddPuts(lcdfd, vetor_menu02[index], 0);
                    }
                    else {
                        // lcdPuts(lcdfd, "UNIDADE INALCANCAVEL :/");
                        // delay(3000);
                        // lcdClear(lcdfd);
                        lcdddPuts(lcdfd, "Unreachable Unit", TWO_SECONDS);
                        // lcdPrintf(lcdfd, "%s", vetor_menu01[index]);
                        // delay(3000);
                        lcdddPuts(lcdfd, vetor_menu01[index], 0);
                    }
                }

            }else if (menu2Active) {
                choiceMenu2 = index;
                // Verificar se apertou enter na posição Voltar
                if (choiceMenu2 == TURN_BACK) {
                    // Desseleciona a Node previamente selecionada
                    // lcdClear(lcdfd);
                    // lcdPrintf(lcdfd, "DESELECIONANDO A UNIDADE %d", selectedNode+1);
                    // delay(3000);
                    lcdddPuts(lcdfd, "Deselecting the unit...", TWO_SECONDS);
                    sendData(uartfd, deselectNode, selectedNode);
                    recvData = recvDigitalData(uartfd);
                    printf("DESELECT RECV DATA -> %d\n", recvData);
                    //serialPutchar(fd, deselect_node[index - 1]);
                    meun1Active = TRUE;
                    menu2Active = FALSE;
                    index = choiceMenu1;
                    //mostrar_menu_01(vetor_menu01, index);
                    // lcdClear(lcdfd);
                    // lcdPuts(lcdfd, "UNIDADE DESELECIONADA");
                    // delay(3000);
                    lcdddPuts(lcdfd, "Unit successfully deselected", TWO_SECONDS);
                    // lcdClear(lcdfd);
                    // lcdPrintf(lcdfd, "%s", vetor_menu01[index]);
                    lcdddPuts(lcdfd, vetor_menu01[index], 0);

                }else{
                     if(MONITORING_ALL) {
                         idxMonitoring = 0;
                         printf("VOCE DECIDIU MONITORAR TODOS OS SENSORES\n");
                         while(digitalRead(BUTTON_DOWN)) {
                             lcdClear(lcdfd);
                             sendData(uartfd, monitoringArray, idxMonitoring);
                             if(idxMonitoring == 2) recvData = recvAnalogData(uartfd);
                             else recvData = recvDigitalData(uartfd);
                             printf("IDX -> %d\n", idxMonitoring);
                             idxMonitoring == 2 ? (idxMonitoring = 0) : idxMonitoring++; 
                             lcdPrintf(lcdfd, "Value %s: %d", monitoringLabels2[idxMonitoring], recvData);
                             lcdPosition(lcdfd, 0, 1);
                             lcdPuts(lcdfd, "<ENTER TO EXIT>");
                             delay(1000);
                         }
                        //  lcdClear(lcdfd);
                        //  lcdPuts(lcdfd, "SAINDO DO MONITORAMENTO");
                        //  delay(1000);
                         while(!digitalRead(BUTTON_DOWN));
                         lcdddPuts(lcdfd, vetor_menu02[index], 0);
                    }
                   if(index == 0) {
                        // lcdClear(lcdfd);
                        // lcdPuts(lcdfd, "LIGANDO O LED BUILTIN...");
                        // delay(2000);
                        lcdddPuts(lcdfd, "Turning on LED", TWO_SECONDS);
                        sendData(uartfd, consultCommands, index);
                        // Verificacao da resposta da UNIDADE
                        continue;
                    }
                    // Opcao de monitorar sensor
                   if(MONITORING) {
                        printf("VOCE ESCOLHEU MONITORAR\n");
                        while(digitalRead(BUTTON_DOWN)) {
                            lcdClear(lcdfd);
                            sendData(uartfd, consultCommands, index - 1);
                            if(MONITORING_ANALOG) recvData = recvAnalogData(uartfd);
                            else recvData = recvDigitalData(uartfd);
                            lcdPrintf(lcdfd, "Value: %d", recvData);
                            lcdPosition(lcdfd, 0, 1);
                            lcdPuts(lcdfd, "<ENTER TO EXIT>");
                            delay(500);
                        }
                        // lcdClear(lcdfd);
                        // lcdPuts(lcdfd, "SAINDO DO MONITORAMENTO");
                        // delay(1000);
                        while(!digitalRead(BUTTON_DOWN));
                        lcdddPuts(lcdfd, vetor_menu02[index], 0);
                    }
                    else if(CONSULT) {
                        // Mandar mensagem para a node e pegar o dado para exibir no LCD
                        // lcdPuts(lcdfd, "ENVIANDO COMANDO...");
                        // delay(1000);
                        lcdddPuts(lcdfd, "Sending Command...", TWO_SECONDS);
                        // Enviando comando a Node selecionada
                        // Logica para implementar a consulta ou monitoramento
                        sendData(uartfd, consultCommands, index);
                        // lcdClear(lcdfd);
                        // lcdPuts(lcdfd, "COMANDO ENVIADO!");
                        // delay(1000);
                        // lcdddPuts(lcdfd, "COMANDO ENVIADO", TWO_SECONDS);
                        lcdddPuts(lcdfd, "Successfully Sent", TWO_SECONDS);
                        if(ANALOG_PIN) { 
                            // LOGICA DO SENSOR ANALOGICO
                            recvData = recvAnalogData(uartfd);
                        }
                        else recvData = recvDigitalData(uartfd);
                        if(recvData > -1) {
                            lcdClear(lcdfd);
                            lcdPrintf(lcdfd, "Sensor Val.: %d", recvData);
                            delay(TWO_SECONDS);
                        }
                        else { 
                            lcdddPuts(lcdfd, "Unreachable Unit", TWO_SECONDS);
                        }
                        //serialPutchar(fd, array_command[menu_counter]);
                        // Mostrar dado que foi recebido
                        // Mostrar Menu 02
                        //mostrar_menu_02(vetor_menu02, index);
                        // lcdClear(lcdfd);
                        // lcdPrintf(lcdfd, "%s", vetor_menu02[index]);
                        lcdddPuts(lcdfd, vetor_menu02[index], 0);
                    }
                }   
            }
        }
    }
    return EXIT_SUCCESS;
}

