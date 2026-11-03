# Referência musical 0.3A

`standalone_03a.json` identifica commits, toolchain, perfil, hashes e métricas da
janela física documentada em `../standalone_hardware_validation_03a_results.md`.
O sdkconfig completo e o lock de componentes são cópias da referência preservada.
Não são defaults de build: contêm gates elétricos habilitados para a unidade
qualificada pelo operador. Nenhum ambiente os carrega automaticamente.

Verificação somente de leitura, a partir da raiz deste worktree:

```powershell
python smk-p4/tests/verify_03a_reference.py
python smk-p4/tests/verify_03a_reference.py --image-dir <diretorio-explicito-do-bundle>
python smk-p4/tests/verify_03a_reference.py --sdkconfig <sdkconfig-gerado> --config-header <config/sdkconfig.h-gerado>
```

O bundle original está em `build/p403a/removal_grace_image` do worktree musical
original. Binários ignorados pelo Git não aparecem em novos clones/worktrees;
forneça o caminho explicitamente e confirme os quatro hashes antes de utilizá-los.
Sem `--image-dir`, o checker declara a validação binária NOT RUN. O checker não
procura arquivos em outros worktrees, não modifica configuração e não grava Flash.
Uma compilação equivalente pode ter hash diferente por caminho/metadados Git;
somente o hash registrado identifica o binário que recebeu a validação física.

Conferir só o arquivo sdkconfig solicitado não prova que o compilador o utilizou.
O PlatformIO pode manter o header anterior se uma cópia conservar mtime anterior
ao CMakeCache. Depois de copiar um perfil, atualizar o mtime do sdkconfig de
trabalho, recompilar e conferir também o header gerado. O checker rejeita a
divergência e, sem --config-header, declara essa verificação NOT RUN.

Roteiro de rollback/build e limites de autorização em
`../firmware_integration_readiness_03b.md`.
