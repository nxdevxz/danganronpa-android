# Danganronpa Android

Projeto experimental para portar **Danganronpa: Trigger Happy Havoc Anniversary Edition** do Nintendo Switch para Android. A versão **0.0.1** introduz o launcher e o pacote independente `io.github.nxdevxz.danganronpa`.

O menu, o ícone e a pasta de dados usam a identidade do Danganronpa. A execução do jogo, o importador e a compatibilidade de saves e shaders ainda precisam de adaptação e teste.

## Compilação

O workflow [Danganronpa Android](.github/workflows/build-danganronpa.yml) gera o artefato `Danganronpa-0.0.1` com `Danganronpa.apk`. A assinatura requer os secrets `DANGANRONPA_SIGNING_KEY_B64` e `DANGANRONPA_SIGNING_PASSWORD` neste repositório. A [verificação de código](.github/workflows/check-danganronpa.yml) roda a cada push e pull request.

O projeto deriva do [P4G Android](https://github.com/nxdevxz/p4g-android). Consulte [LICENSE](LICENSE) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) para licenças e créditos.
