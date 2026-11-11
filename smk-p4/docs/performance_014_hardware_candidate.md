# SMK-P4 0.14 — candidatos ESP32-P4

## Ambientes

- guition-editor-8 usa os gates elétricos desabilitados para build e revisão.
- guition-editor-8-bench mantém a configuração documentada da bancada P4, incluindo USB controlador 0 e I²S nos GPIOs 50/51/52. Qualificar alimentação VBUS, placa, interface USB e DAC da unidade antes de qualquer execução física.

A verificação usa o script check_blueprint_build.py com oito vozes, biblioteca completa, Harmony, editor e as flags AMY 48 kHz / 256 frames. CPU, heap e PSRAM livre em execução dependem da placa e da cena musical.

## Reprodução do build

Na raiz do repositório, executar os ambientes em sequência.

```powershell
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8-bench -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench
```

## Resultados

Commit de código compilado 3257f7ab02a85e1aa34efd6290b1e2b50abc3924.

| Ambiente | Build e auditoria | Binário (bytes) | Flash (bytes) | RAM (bytes) | SHA-256 |
|---|---|---:|---:|---:|---|
| guition-editor-8 | PASS | 1260880 | 1260146 | 41240 | 81BEE1F68A3AA4A2AE160A7753F0609C1C3CA49F516CBB38FCDAB5A63056F58E |
| guition-editor-8-bench | PASS | 1361840 | 1361126 | 41624 | 0B01C52554E721262905340980ADF91C012D32E6B9CEDB471D878995BFF28469 |

Flash e RAM são os valores informados pelo PlatformIO. Os dois perfis compilam com PSRAM habilitada. Heap e PSRAM livre em execução, prazos e áudio são MANUAL.

Os binários e o manifesto verificado estão em build/performance014/candidates.

NO-GO para release. A validação de bancada e os itens de software documentados no inventário permanecem abertos.

## Procedimento manual

1. Conferir o perfil e o SHA-256 do binário. Registrar placa, versão, alimentação e backup de NVS e aplicação. Qualquer gravação precisa de autorização explícita para esse SHA-256.
2. Após boot autorizado, registrar ADMISSION, USB e os diagnósticos de render/PCM. Confirmar oito vozes, 48 kHz e 256 frames.
3. Em Juno e DX7, sustentar oito notas, mover cada macro e retornar ao neutro. Conferir posição confirmada, efeito audível, algoritmo FM preservado e recuperação dos valores manuais. Repetir com chorus ativo, delay e reverb.
4. Na aba MIDI, armar Learn em um knob por vez e registrar canal, CC e cabo do SMK25 V2. Confirmar os oito bindings. Capturar a mensagem de KNOB-B separadamente.
5. Com o knob distante do valor interno, trocar de banco e de patch. Verificar a direção indicada, cruzar o alvo e conferir que a captura não muda o valor. Repetir após edição por toque, Compare/Revert e Recall.
6. Enquanto move os knobs rapidamente, tocar acordes, liberar teclas com e sem sustain, usar pitch bend e CC1, desconectar o USB durante notas e reconectar. Confirmar limpeza das vozes USB, retorno sem reboot e funcionamento contínuo do toque.
7. Salvar perfil e preset com oito posições distintas. Reiniciar, conferir banco, bindings, posições e FX, e recuperar presets V1/V2 de uma cópia do acervo. Monitorar deadline, underruns, heap e PSRAM durante o ensaio.
8. Em caso de regressão, parar o ensaio e comparar os logs. O rollback usa a aplicação 0.13.1 verificada e, para restaurar exatamente o conteúdo pré-ensaio, o backup de NVS da própria unidade. Qualquer gravação de rollback também requer autorização pelo SHA-256 específico.

A referência histórica do rollback está em [stability_0131_hardware_evidence.md](stability_0131_hardware_evidence.md).
