# Pads e atalhos no SMK-P4 0.14.2

O perfil ativo, o dispositivo USB identificado e o canal 10 delimitam estes bindings. O host publica tanto Note Off quanto Note On com velocity zero como soltura. A UI mostra número, banco, velocity de ataque e estado. Os pads do banco A são monitorados e reservados para futuros destinos musicais.

| Pad | Nota MIDI | Ação |
|---|---:|---|
| A1 | 36 | Monitor |
| A2 | 37 | Monitor |
| A3 | 38 | Monitor |
| A4 | 39 | Monitor |
| A5 | 40 | Monitor |
| A6 | 41 | Monitor |
| A7 | 42 | Monitor |
| A8 | 43 | Monitor |
| B1 | 44 | Patch anterior |
| B2 | 45 | Próximo patch |
| B3 | 46 | Home |
| B4 | 47 | ARPEGGIATOR NOT AVAILABLE |
| B5 | 48 | Página anterior |
| B6 | 49 | Próxima página |
| B7 | 50 | MIDI Settings / Learn |
| B8 | 51 | Abrir Save As ao soltar após pelo menos 1.200 ms |

Patch anterior/próximo usa a mesma seleção e confirmação CANCEL / DISCARD & LOAD do toque. MIDI Settings abre sem armar a captura. Save As abre o painel existente para selecionar nome e slot e confirmar a operação. Duplicação do Note On, perda na fila de controle, desconexão e mudança de geração invalidam a pressão longa. Um Note Off correspondente é obrigatório, inclusive se a pressão já excedeu 1.200 ms. A confiabilidade da soltura no controlador físico segue pendente.
