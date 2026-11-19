# Changelog

## 0.5.2 Beta

### Launcher e controles

- Configurações do launcher e Monitor de desempenho agora têm seções próprias na tela principal.
- As opções de gráficos e controles abrem listas com os valores disponíveis. O botão Jogar permanece fixo.
- O menu durante o jogo mantém apenas o ajuste rápido dos controles na tela. O limite de FPS continua disponível no launcher.
- O port voltou ao renderizador anterior do P4G. Foram retirados os ajustes experimentais de FSR, sombras, bloom e HUD em alta resolução.

### Preparação e armazenamento

- A preparação permite escolher entre instalar em Android/data e jogar diretamente pelo NSP/XCI.
- Se o arquivo de atualização ficar inacessível no modo direto, é possível selecioná-lo novamente ou preparar apenas o jogo base.
- A tela de preparação mostra em português o progresso da extração, a geração de shaders e o espaço livre.

### Verificação e configurações

- Ao voltar ao launcher, uma instalação verificada nos últimos cinco minutos pode passar pela checagem rápida. Alterações no manifesto, preparação ou falha ao abrir o jogo exigem nova verificação completa.
- O launcher só substitui o arquivo de configurações depois de concluir a gravação, preservando os ajustes anteriores se ela falhar.
