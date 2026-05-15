#pragma once

#include <stdint.h>

// Definições opacas para estruturas do IL2CPP
typedef void Il2CppDomain;
typedef void Il2CppAssembly;
typedef void Il2CppImage;
typedef void Il2CppClass;
typedef void Il2CppObject;

// Estrutura MethodInfo (simplificada, tratada como opaca na maioria dos casos, mas precisamos do ponteiro)
struct MethodInfo;

// Ponteiros para funções da API do IL2CPP
// Estas variáveis serão preenchidas dinamicamente via dlsym
extern "C" {
    typedef const char* (*Func_il2cpp_method_get_name)(const MethodInfo* method);
    typedef Il2CppClass* (*Func_il2cpp_method_get_class)(const MethodInfo* method);
    typedef const char* (*Func_il2cpp_class_get_name)(Il2CppClass* klass);
    typedef const char* (*Func_il2cpp_class_get_namespace)(Il2CppClass* klass);
    
    // Assinatura do il2cpp_runtime_invoke
    // Object* il2cpp_runtime_invoke(MethodInfo *method, void *obj, void **params, Il2CppException **exc);
    typedef Il2CppObject* (*Func_il2cpp_runtime_invoke)(const MethodInfo* method, void* obj, void** params, void** exc);
}

// Variáveis globais para armazenar os endereços das funções originais
extern Func_il2cpp_method_get_name il2cpp_method_get_name;
extern Func_il2cpp_method_get_class il2cpp_method_get_class;
extern Func_il2cpp_class_get_name il2cpp_class_get_name;
extern Func_il2cpp_class_get_namespace il2cpp_class_get_namespace;
extern Func_il2cpp_runtime_invoke il2cpp_runtime_invoke;
