# ApexCombatMod

**Mod Android para Apex Combat Online, antigo Air Combat Online.**

**Idioma:** Português (Brasil)

---

## ApexCombatOnline 6.4.0 (mudanças importantes)

Nesta versão do jogo (6.4.0), o app passou a exigir **Split APKs** para instalar/rodar corretamente (ex.: `split_config.arm64_v8a.apk` e `split_UnityDataAssetPack.apk`). Por isso:

- Não dá para “embutir tudo” em um único `.apk` instalável via `adb install`.
- O output correto para compartilhar/instalar é:
  - um **arquivo único** `.apks` (é um `.zip` renomeado), para instalar com um instalador de splits (ex.: SAI), ou
  - a pasta `...-Splits/` para instalar com `adb install-multiple`.

Além disso, a validação de licença/assinatura mudou e passou a exigir:

- patch de licença no `AndroidManifest.xml` (remoção de componentes do `com.pairip.licensecheck`)
- patch de verificação de assinatura em smali (aplicado automaticamente ao descompilar a base)

## Estrutura do Projeto

```
ApexCombatMod/
├── app/src/main/java/com/on00dev/apexcombatmod/   # Código Java do mod
│   ├── FloatingModMenuService.java   # Menu flutuante (UI)
│   ├── MainActivity.java              # Activity principal
│   ├── ModLoader.java                 # Carregador do mod
│   └── Native.java                    # Interface JNI
├── app/src/main/cpp/                  # Código nativo (C++)
│   └── native-lib.cpp                 # Hooks e patches
├── assets_decompiled/                 # Assets extraídos do base.apk (pasta assets/)
├── config_decompiled/                 # Overrides de arquivos do APK (ex: lib/)
├── base_decompiled/                   # base.apk descompilado via apktool (para patches em smali/manifest)
├── original_splits/                   # Conjunto do APK original (base.apk + splits, se existirem)
├── extracted/                         # Arquivos extraídos (lua, etc)
├── apk_final/                         # APK final gerado
├── dist/                              # Artefatos intermediários (ex: base_mod-unsigned.apk)
├── ApexCombat-Mod-*-dumped/           # Dump(s) do IL2CPP (dump.cs) por versão/ABI
├── prepare_base.ps1                   # Prepara base: splits + assets + base_decompiled
├── build_final.ps1                   # Build final: aplica mod + rebuild + merge + assinatura
└── cleanup_workspace.ps1              # Script de limpeza
```

---

## Requisitos

- Windows + PowerShell
- Java (para apktool)
- apktool no PATH
- Python (usado no merge final de APKs)
- Android SDK Build-Tools (para `zipalign` e `apksigner`)
  - `local.properties` é opcional
  - o script tenta localizar via `ANDROID_SDK_ROOT`/`ANDROID_HOME` e caminhos padrão do Windows
- ADB (opcional, para instalar e para fallback de extração de splits do device)

## Builds

### Build de Debug (rápido)

```powershell
.\gradlew.bat assembleDebug
```

Gera APK em: `app/build/outputs/apk/debug/app-debug.apk`

### Build Final (merge + assinatura)

Gera o APK completo com o mod mesclado no jogo original.

```powershell
.\build_final.ps1
```

**Saída:** `apk_final/ApexCombat-Mod-VXX-Final.apk`
**Saída (para splits):** `apk_final/ApexCombat-Mod-VXX.apks` e `apk_final/ApexCombat-Mod-VXX-Splits/`

O script:
1. Compila o mod (release)
2. Descompila o APK do mod e a base
3. Copia o código smali do mod para a base
4. Remove licença do jogo
5. Aplica patch de verificação de assinatura (smali)
6. Faz rebuild com apktool
7. Merge dos assets/libs
8. Assina com keystore de debug

---

## Como gerar o APK final (workflow recomendado)

### 1) Atualize a base do jogo

Coloque o APK atualizado do jogo na raiz como `base.apk` ou informe o caminho no comando.

Se você tiver um pacote `.apks` (bundletool), você também pode usar ele como entrada.

### 2) Prepare as pastas de trabalho (splits + assets + base_decompiled)

```powershell
# Caso você tenha base.apk na raiz do projeto:
.\prepare_base.ps1

# Caso o base.apk esteja em outro lugar:
.\prepare_base.ps1 -InputApkOrApks "C:\caminho\para\base.apk"

# Caso você tenha um arquivo .apks:
.\prepare_base.ps1 -InputApkOrApks "C:\caminho\para\jogo.apks"

# Também aceita .xapk/.zip (desde que contenha base + splits):
.\prepare_base.ps1 -InputApkOrApks "C:\caminho\para\jogo.xapk"

# Ou uma pasta que contenha base.apk + split_config.*.apk:
.\prepare_base.ps1 -InputApkOrApks "C:\caminho\para\pasta_com_apks"

# Fallback via ADB (se o jogo estiver instalado no device na versão original):
.\prepare_base.ps1 -PackageName "com.vector.apexcombat.google"
```
*Recomendado fazer preparação via ADB*

Isso vai:
- Popular `original_splits/` com o conjunto original (sempre garantindo `original_splits/base.apk`).
- Extrair `assets/` do `base.apk` para `assets_decompiled/assets/...`.
- Descompilar o `base.apk` para `base_decompiled/` e aplicar:
  - patch de licença no manifest
  - patch de verificação de assinatura (smali)

Se o jogo estiver usando split APKs, não existe como “extrair os splits de dentro do base.apk”. Você precisa do bundle completo (base + splits) ou usar o fallback via ADB.

### 3) Gere o APK final com o mod

```powershell
.\build_final.ps1
```

O `build_final.ps1` usa por padrão `original_splits/base.apk` como base (se você não passar `-BaseApk`).

Se a versão do jogo estiver usando split APKs, instalar apenas o `ApexCombat-Mod-VXX-Final.apk` pode falhar com:

```
INSTALL_FAILED_MISSING_SPLIT
```

Nesse caso, use uma destas opções:

1) Instalar via ADB (recomendado para debug)

O `build_final.ps1` gera uma pasta com o conjunto completo:

```
apk_final/ApexCombat-Mod-VXX-Splits/
```

E imprime no terminal o comando `adb install-multiple ...` já pronto.

2) Compartilhar com outra pessoa (arquivo único)

O `build_final.ps1` também gera um arquivo `.apks`:

```
apk_final/ApexCombat-Mod-VXX.apks
```

Esse arquivo pode ser instalado com um instalador de splits (ex.: SAI).

Para apenas gerar os arquivos (sem instalar), use:

```powershell
.\build_final.ps1 -SkipInstall
```

---

## Atualizar offsets (IL2CPP) após update do jogo

Quando o jogo atualiza, os offsets do `libil2cpp.so` mudam. O fluxo recomendado:

1) Extraia o dump (ex.: `ApexCombatOnline6.4.0_dumped/arm64-v8a/dump.cs`)
2) Atualize os `OFFSET_...` em `app/src/main/cpp/native-lib.cpp` com base no `dump.cs`
3) Recompile o mod e gere o APK final

## Limpar Workspace

Remove arquivos gerados, builds temporários e mantém apenas os essenciais.

```powershell
# Modo normal (remove tudo)
.\cleanup_workspace.ps1

# DryRun (mostra o que seria removido)
.\cleanup_workspace.ps1 -DryRun

# Manter mais de 1 APK final
.\cleanup_workspace.ps1 -KeepFinalCount 3
```

---

## Onde Modificar

### Adicionar/Modificar Hooks e Patches

**Arquivo:** `app/src/main/cpp/native-lib.cpp`

```cpp
// --- OFFSETS ---
// Procure "OFFSET_" para encontrar os endereços de memória

// --- HOOKS ---
// Procure "hook_" para modificar o comportamento das funções hooked

// --- MEMORY PATCHES ---
// Procure "MemoryPatch" para adicionar novos patches
// Exemplo:
MemoryPatch patchNovo("NovoPatch", 0x123456);
patchNovo.ApplyFloat1(libIl2CppBase);  // Retorna 1.0f
patchNovo.ApplyTrue(libIl2CppBase);    // Retorna true
patchNovo.Apply(libIl2CppBase);        // RET (void)
```

### Adicionar Funcionalidade ao Menu

**Arquivo:** `app/src/main/java/com/on00dev/apexcombatmod/FloatingModMenuService.java`

```java
// Adicionar novo switch:
Switch swNovo = createSwitch("Nova Função");
swNovo.setOnCheckedChangeListener((buttonView, isChecked) -> {
    Native.SetNovaFuncao(isChecked);  // Chama função nativa
});
menuLayout.addView(swNovo);
```

**Arquivo:** `app/src/main/java/com/on00dev/apexcombatmod/Native.java`

Adicione novos métodos JNI:
```java
public static native void SetNovaFuncao(boolean isEnabled);
```

**Arquivo:** `app/src/main/cpp/native-lib.cpp`

Implemente a função nativa:
```cpp
extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_SetNovaFuncao(JNIEnv *env, jclass type, jboolean isEnabled) {
    // Implemente a lógica aqui
}
```

---

## Pastas Descompiladas

> ⚠️ **Nota:** As pastas descompiladas são muito grandes (~2GB) e por isso não estão no repositório.
> Baixe os arquivos do **Release** e extraia no workspace antes de usar.

### Download dos Arquivos

Baixe os arquivos `.zip` do [release](https://github.com/ON00dev/ApexCombatMod/releases/tag/data_workspace) e extraia-os na raiz do projeto:

```powershell
# Após extrair, a estrutura deve ficar:
# ApexCombatMod/
# ├── assets_decompiled/
# ├── config_decompiled/
# ├── base_decompiled/
# ├── original_splits/
# ├── apk_final/
# └── extracted/
```

### Descrição das Pastas

| Pasta | Descrição |
|-------|-----------|
| `assets_decompiled/` | Assets extraídos do jogo (usa `assets_decompiled/assets/...` no merge) |
| `config_decompiled/` | Overrides para o merge final (ex: `config_decompiled/lib/...`) |
| `base_decompiled/` | Base descompilada via apktool para aplicar patches/ajustes no APK |
| `original_splits/` | Conjunto do APK original (inclui `base.apk`; pode conter splits) |
| `extracted/` | Arquivos extraídos (scripts Lua, etc) |
| `dist/` | Artefatos intermediários do build (APK rebuildado sem merge/assinatura final) |

Essas pastas são usadas pelo script `build_final.ps1` para fazer o merge do mod com o jogo.

---

## APK Final

O APK final está em:

```
apk_final/ApexCombat-Mod-VXX-Final.apk
```

Versionamento automático: a cada build, incrementa a versão (V48, V49, V50...).

---

## Dump para Análise

**Arquivo:** `dump.cs`

Contém um dump de classes e métodos do jogo para análise de reversão. Gerado via Il2CppDumper ou ferramenta similar.

Para atualizar o dump:
1. Extraia `libil2cpp.so` e `global-metadata.dat` do APK
2. Execute o Il2CppDumper
3. Substitua o `dump.cs` resultante

---

## Requisitos

- Android Studio
- Android NDK
- JDK 11+
- Python (para build_final.ps1)
- Apktool (instalado no PATH)
- Build Tools do Android SDK

Observações:
- O `build_final.ps1` tenta localizar o Android SDK automaticamente (ANDROID_SDK_ROOT / ANDROID_HOME / caminhos padrão). Se preferir, você pode criar `local.properties` com `sdk.dir=...`.

---

## Licença

[MIT](/LICENSE)
 
