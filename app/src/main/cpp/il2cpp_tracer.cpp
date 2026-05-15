#include <jni.h>
#include <android/log.h>

#define TAG "ApexMod"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

// O tracer foi desativado porque estava sobrescrevendo os valores localmente 
// e causando bugs de "1% HP" sem alterar o comportamento no servidor.
// Todo o hack foi movido para o native-lib.cpp usando patches de memoria que
// não interferem na UI padrão.

__attribute__((constructor))
void lib_main() {
    LOGD("Tracer desativado. Usando apenas Mod Menu (native-lib.cpp).");
}
