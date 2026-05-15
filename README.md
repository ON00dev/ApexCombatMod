# ApexCombatMod

**Mod Android para Apex Combat Online, antigo Air Combat Online.**

**Idioma:** Português (Brasil)

---

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
├── assets_decompiled/                 # Assets descompilados do jogo
├── config_decompiled/                 # Configurações descompiladas
├── base_decompiled/                   # APK base do jogo descompilado
├── original_splits/                   # APK original do jogo
├── extracted/                         # Arquivos extraídos (lua, etc)
├── apk_final/                         # APK final gerado
├── dump.cs                            # Dump de memória para análise
├── build_final.ps1                   # Script de build final
└── cleanup_workspace.ps1              # Script de limpeza
```

---

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

O script:
1. Compila o mod (release)
2. Descompila o APK do mod e a base
3. Copia o código smali do mod para a base
4. Remove licença do jogo
5. Faz rebuild com apktool
6. Merge dos assets/libs
7. Assina com keystore de debug

---

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
| `assets_decompiled/` | Recursos do APK (imagens, textos, configs) |
| `config_decompiled/` | Configurações lib/ (libs nativas) |
| `base_decompiled/` | APK base do jogo descompilado |
| `original_splits/` | APK original do jogo (split APKs) |
| `extracted/` | Arquivos extraídos (scripts Lua, etc) |

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

---

## Licença

[MIT](/LICENSE)
 
