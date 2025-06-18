# Raikeb Volume Card Mixer 🔊🎶 
<img src="https://github.com/user-attachments/assets/22a34571-ea79-48ca-8a6e-293ce29120e4" alt="iconeRaikebVolumeCardMixer" style="width: 270px; height: auto;" />
<img src="https://github.com/user-attachments/assets/e9d3aff4-750a-4319-90ff-4ff71c17f28a" alt="iconeRaikebVolumeCardMixer" style="width: 270px; height: auto;" />

## What is Raikeb Volume Card Mixer 📢

RaikebVolumeCardMixer is a lightweight utility for Windows that provides visual feedback when adjusting the volume of applications via any physical volume mixer that directly relates to the sound of open processes. It displays a sleek notification card showing which application's volume was changed and its new volume level.

![image](https://github.com/user-attachments/assets/c199314e-1987-4116-97dc-5360d47a54a4)
<img src="https://github.com/user-attachments/assets/16e78f3f-2d3a-4543-936c-64f45caae502" alt="iconeRaikebVolumeCardMixer" style="width: 220px; height: auto;" />


## Features ✨

- Real-time volume change detection
- Clean, modern notification cards
- Customizable display position (4 corners of the screen)
- Multi-monitor support
- Startup option with silent mode

## Technologies Used 🛠

1. **Windows API (Win32)**: Core foundation for interacting with Windows system and audio controls
   - Provides low-level access to audio sessions and volume controls
   - Enables system tray integration and window management

2. **Core Audio APIs (MMDeviceAPI, EndpointVolume, AudioSession)**: 
   - `IMMDeviceEnumerator`: Enumerates audio devices
   - `IAudioSessionManager2`: Manages audio sessions
   - `ISimpleAudioVolume`: Controls per-application volume levels
   - Chosen for precise volume monitoring and control

3. **COM (Component Object Model)**:
   - Required for Core Audio API integration
   - Enables communication between components in Windows

4. **DWM API**:
   - Used for creating translucent, non-rectangular windows
   - Enables the modern card-style notifications

## Configuration Options ⚙️

1. **Launch on Startup**:
   - Automatically starts with Windows (adds to registry Run key)

2. **Start Windowed**:
   - Opens minimized to system tray if unchecked
   - Opens settings window if checked

3. **Preferred Display**:
   - Select which monitor to show volume cards on (multi-monitor setups)

4. **Card Popup Location**:
   - Choose between 4 screen corners for notification position:
     - Bottom right (default)
     - Top right
     - Bottom left
     - Top left

## Installation 📥

1. Download the latest release from [Releases page]([url](https://github.com/Raikeb/RaikebVolumeCardMixer/releases))
2. Unzip the RaikebVolumeCardMixer file to the desired location
3. Run `RaikebVolumeCardMixer.exe`
4. Configure your preferences in the settings window
5. The program will minimize to system tray and work silently

## Windows Defender 🦾🤖

Since this application interacts directly with the detection of process sounds, the operating system obviously activates a warning flag for the executable. So don't worry and run the program. If something is blocked, go to Windows Defender and restore it. If you still have concerns, feel free to check the code available here on git.

<img src="https://github.com/user-attachments/assets/dc2b5099-91ec-43cd-b909-310bdbd6851c" alt="iconeRaikebVolumeCardMixer" style="width: 350px; height: auto;" />
<img src="https://github.com/user-attachments/assets/f36d9bd4-89cd-464e-a60b-0aa9fdaadae6" alt="iconeRaikebVolumeCardMixer" style="width: 350px; height: auto;" />


<img src="https://github.com/user-attachments/assets/89b68418-7414-44b3-aff1-0776f44f34c0" alt="iconeRaikebVolumeCardMixer" style="width: 450px; height: auto;" />
<img src="https://github.com/user-attachments/assets/ed0ad348-d953-4fbf-8c7b-7db6de087bf3" alt="iconeRaikebVolumeCardMixer" style="width: 320px; height: auto;" />


## Connect With Me 🤝

- GitHub: [github.com/Raikeb](https://github.com/Raikeb)
- Twitter/X: [@Raikeb](https://x.com/Raikeb)
- LinkedIn: [linkedin.com/in/raikeb](https://www.linkedin.com/in/raikeb/)

---

# Raikeb Volume Card Mixer (Português) 🔊🎶
<img src="https://github.com/user-attachments/assets/22a34571-ea79-48ca-8a6e-293ce29120e4" alt="iconeRaikebVolumeCardMixer" style="width: 270px; height: auto;" />
<img src="https://github.com/user-attachments/assets/e9d3aff4-750a-4319-90ff-4ff71c17f28a" alt="iconeRaikebVolumeCardMixer" style="width: 270px; height: auto;" />

## O que é o Raikeb Volume Card Mixer 📢

O RaikebVolumeCardMixer é um utilitário leve para Windows que fornece feedback visual ao ajustar o volume de aplicativos por meio de qualquer mixer de volume físico que se relacione diretamente com o som de processos abertos. Ele exibe um cartão de notificação elegante mostrando qual aplicativo teve o volume alterado e seu novo nível de volume.

![image](https://github.com/user-attachments/assets/c199314e-1987-4116-97dc-5360d47a54a4)
<img src="https://github.com/user-attachments/assets/16e78f3f-2d3a-4543-936c-64f45caae502" alt="iconeRaikebVolumeCardMixer" style="width: 220px; height: auto;" />

## Funcionalidades ✨

- Detecção em tempo real de mudanças de volume
- Cards de notificação modernos e limpos
- Posição personalizável (4 cantos da tela)
- Suporte a múltiplos monitores
- Opção de iniciar com o Windows em modo silencioso

## Tecnologias Utilizadas 🛠

1. **Windows API (Win32)**: Base para interação com o sistema Windows
   - Fornece acesso de baixo nível a sessões de áudio e controles de volume
   - Permite integração com a bandeja do sistema e gerenciamento de janelas

2. **Core Audio APIs (MMDeviceAPI, EndpointVolume, AudioSession)**:
   - `IMMDeviceEnumerator`: Enumera dispositivos de áudio
   - `IAudioSessionManager2`: Gerencia sessões de áudio
   - `ISimpleAudioVolume`: Controla níveis de volume por aplicativo
   - Escolhidas para monitoramento preciso do volume

3. **COM (Component Object Model)**:
   - Necessário para integração com as Core Audio APIs
   - Permite comunicação entre componentes no Windows

4. **DWM API**:
   - Usada para criar janelas translúcidas e não-retangulares
   - Possibilita os cards de notificação modernos

## Opções de Configuração ⚙️

1. **Iniciar com o Windows**:
   - Inicia automaticamente com o Windows (adiciona à chave Run do registro)

2. **Iniciar Janelado**:
   - Abre minimizado na bandeja do sistema se desmarcado
   - Abre a janela de configurações se marcado

3. **Monitor Preferido**:
   - Seleciona em qual monitor exibir os cards (para setups multi-monitor)

4. **Posição do Card**:
   - Escolha entre 4 cantos da tela para a notificação:
     - Inferior direito (padrão)
     - Superior direito
     - Inferior esquerdo
     - Superior esquerdo

## Instalação 📥

1. Baixe a versão mais recente na [página de Releases]([url](https://github.com/Raikeb/RaikebVolumeCardMixer/releases))
2. Descompate o arquivo RaikebVolumeCardMixer no local que desejar
3. Execute `RaikebVolumeCardMixer.exe`
4. Configure suas preferências na janela de configurações
5. O programa será minimizado para a bandeja do sistema e funcionará silenciosamente


## Windows Defender 🦾🤖

Como essa aplicação interage diretamente com a deteção do som dos processos, obviamente o sistema operacional ativa uma flag de alerta para o executável. Então fique tranquilo e execute o programa, caso algo seja bloqueado vá no Windows Defender e restaure. Caso ainda fique preocupado, sinta-se livre para averiguar o código disponibilizado aqui no git.

<img src="https://github.com/user-attachments/assets/dc2b5099-91ec-43cd-b909-310bdbd6851c" alt="iconeRaikebVolumeCardMixer" style="width: 350px; height: auto;" />
<img src="https://github.com/user-attachments/assets/f36d9bd4-89cd-464e-a60b-0aa9fdaadae6" alt="iconeRaikebVolumeCardMixer" style="width: 350px; height: auto;" />


<img src="https://github.com/user-attachments/assets/89b68418-7414-44b3-aff1-0776f44f34c0" alt="iconeRaikebVolumeCardMixer" style="width: 450px; height: auto;" />
<img src="https://github.com/user-attachments/assets/ed0ad348-d953-4fbf-8c7b-7db6de087bf3" alt="iconeRaikebVolumeCardMixer" style="width: 320px; height: auto;" />



## 🤝 Conecte-se Comigo

- GitHub: [github.com/Raikeb](https://github.com/Raikeb)
- Twitter/X: [@Raikeb](https://x.com/Raikeb)
- LinkedIn: [linkedin.com/in/raikeb](https://www.linkedin.com/in/raikeb/)
