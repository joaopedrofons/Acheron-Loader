# 🛡️ Acheron-Loader

> Framework modular de pós-exploração e execução fileless para Windows x64, com foco em evasão de EDRs modernos.

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11%20x64-0078D6.svg)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/Build-Visual%20Studio%202022-purple.svg)](https://visualstudio.microsoft.com/)

---

## 📖 Visão Geral

**Acheron-Loader** é um framework de pós-exploração escrito em **C++ puro** para **Windows x64**. Ele implementa técnicas avançadas de evasão de EDRs, execução fileless e coleta furtiva de credenciais — tudo sem tocar o disco rígido.

O projeto foi desenvolvido como um **laboratório de engenharia reversa** e **pesquisa em segurança ofensiva**, com foco em:

- **Fileless execution** — execução direta em memória RAM
- **Evasion de EDRs** — bypass de hooks de user-mode e kernel
- **Reconnaissance furtivo** — coleta de credenciais com APIs NT
- **Portfólio técnico** — demonstração de domínio em Windows Internals

---

## 🏗️ Arquitetura

```
┌──────────────────────────────────────────────────────────────┐
│                       ACHERON-LOADER                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────────┐   ┌────────────────┐   ┌─────────────┐   │
│  │   PE Loader    │   │  SyscallMgr    │   │  AntiEDR    │   │
│  │  (Core)        │   │  (Core)        │   │  (Evasion)  │   │
│  └────────┬───────┘   └────────┬───────┘   └──────┬──────┘   │
│           │                    │                   │         │
│           └────────────────────┼───────────────────┘         │
│                                │                             │
│                    ┌───────────▼────────────┐                │
│                    │   Execution Engine     │                │
│                    └───────────┬────────────┘                │
│                                │                             │
│           ┌────────────────────┼────────────────┐            │
│           │                    │                │            │
│    ┌──────▼──────┐     ┌───────▼──────┐   ┌─────▼─────┐      │
│    │  Scanner    │     │  C2 Client   │   │  Logger   │      │
│    │  (Payload)  │     │  (Network)   │   │  (Utils)  │      │
│    └─────────────┘     └──────────────┘   └───────────┘      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 🔧 Módulos

### 📦 PE Loader (`src/core/pe_loader.cpp`)

Carregador manual de arquivos PE (DLL/EXE) em memória, sem uso de `LoadLibrary` ou `CreateProcess`.

- Parsing manual de `IMAGE_DOS_HEADER`, `IMAGE_NT_HEADERS`, `IMAGE_SECTION_HEADER`
- Resolução de **IAT** (Import Address Table)
- Aplicação de **relocations** (ASLR)
- **Memory Hygiene**: `.text` como `PAGE_EXECUTE_READ`, dados como `PAGE_READWRITE` (nunca RWX)

### 🔄 SyscallManager (`src/core/syscalls_advanced.cpp`)

Execução de syscalls indiretos para bypassar hooks de EDRs.

- **Resolução dinâmica** de 976 syscalls do `ntdll.dll`
- Extração de **SSN** (System Service Number) via parsing da export table
- Execução via **shellcode gerado em memória** (`mov eax, SSN; syscall; ret`)
- **Fallback** para `GetProcAddress` em syscalls com >4 argumentos

### 🛡️ AntiEDR (`src/evasion/anti_edr.cpp`)

Camada de evasão e anti-análise.

- **Anti-debug**: PEB `BeingDebugged`, `NtGlobalFlag`, hardware breakpoints
- **Detecção de sandbox**: processos de VM, CPUs < 2, RAM < 2GB
- **Detecção de EDRs**: CrowdStrike, SentinelOne, Defender, Cylance, etc.
- **AMSI/ETW bypass**: hardware breakpoints em `AmsiScanBuffer` e `EtwEventWrite`
- **Unhook do ntdll**: restauração dos bytes originais do disco
- **Module Stomping**: execução em seção `.text` de DLLs legítimas
- **Code Caves**: alocação de stubs em espaços vazios de `.text`

### 📡 StealthScanner (`src/payloads/stealth_scanner.cpp`)

Scanner furtivo de credenciais em arquivos sensíveis.

- Varredura de diretórios com `FindFirstFile` / `FindNextFile`
- Detecção de arquivos sensíveis (`.env`, `.config`, `.json`, `.xml`, `.ini`)
- Extração de credenciais via **regex** (`api_key`, `password`, `token`, AWS keys)
- Exportação para **JSON**

### 🌐 C2 Client (`src/core/c2_client.cpp`)

Cliente HTTP para exfiltração de dados.

- Envio via **HTTPS** (WinHTTP)
- User-Agent legítimo para mimetismo
- (Em desenvolvimento: criptografia AES-256)

### 📋 Logger (`src/utils/logger.cpp`)

Sistema de logging thread-safe.

- Suporte a **wide-char** (UTF-16)
- Alocação dinâmica de buffer
- Níveis: `Debug`, `Info`, `Warn`, `Error`, `Fatal`
- Saída para arquivo e console

---

## 🎯 Mapeamento MITRE ATT&CK

| Técnica | ID | Módulo | Descrição |
|---------|-----|--------|-----------|
| **Reflective Code Loading** | T1620 | PE Loader | PE mapeado na memória sem tocar o disco |
| **Process Injection** | T1055 | SyscallManager | Thread criada via `NtCreateThreadEx` |
| **Obfuscated Files or Information** | T1027 | Logger | Strings de log ofuscadas |
| **Impair Defenses: AMSI Bypass** | T1562.001 | AntiEDR | Hardware breakpoints em AMSI |
| **Impair Defenses: ETW Bypass** | T1562.001 | AntiEDR | Hardware breakpoints em ETW |
| **OS Credential Dumping** | T1003 | StealthScanner | Extração de credenciais via regex |
| **Data from Local System** | T1005 | StealthScanner | Coleta de arquivos sensíveis |
| **Exfiltration Over C2 Channel** | T1041 | C2 Client | Envio via HTTPS |
| **Application Layer Protocol: Web** | T1071.001 | C2 Client | HTTPS com User-Agent legítimo |

---

## 🚀 Como Compilar

### Pré-requisitos

- **Visual Studio 2022** (Community, Professional ou Enterprise)
- **Windows SDK** 10.0.22621.0 ou superior
- **C++17** ou superior
- **Git** (para clonar o repositório)

### Passos

```bash
# 1. Clonar o repositório
git clone https://github.com/joaopedrofons/Acheron-Loader.git
cd Acheron-Loader

# 2. Abrir no Visual Studio
# Abra o arquivo Acheron.sln

# 3. Configurar para Release x64
# No topo do Visual Studio: Release | x64

# 4. Compilar
# Ctrl + Shift + B
```

O executável será gerado em:
```
x64\Release\Acheron.exe
```

---

## 💻 Como Usar

```bash
# Modo scanner (sem payload)
Acheron.exe --recon-only

# Modo normal (com payload padrão)
Acheron.exe

# Modo debug (logs detalhados)
Acheron.exe --debug

# Modo stealth (logs mínimos)
Acheron.exe --stealth

# Especificar payload personalizado
Acheron.exe --payload C:\caminho\payload.dll

# Especificar arquivo de log
Acheron.exe --log C:\temp\acheron.log

# Especificar arquivo de resultados
Acheron.exe --results C:\temp\recon.json

# Definir timeout (em segundos)
Acheron.exe --timeout 600

# Ver ajuda
Acheron.exe --help
```

### 📂 Arquivos gerados

Após a execução, os seguintes arquivos são criados no diretório atual:

| Arquivo | Descrição |
|---------|-----------|
| `acheron.log` | Log completo da execução |
| `recon_results.json` | Credenciais encontradas pelo scanner |

### 📄 Exemplo de `recon_results.json`

```json
{
  "credentials": [
    {
      "file": "C:\\Users\\Usuario\\projeto\\.env",
      "key": "API_KEY",
      "value": "sk-1234567890abcdef"
    },
    {
      "file": "C:\\inetpub\\wwwroot\\config.json",
      "key": "password",
      "value": "admin123"
    }
  ]
}
```

---

## 📁 Estrutura do Projeto

```
Acheron-Loader/
├── Acheron.sln                    # Solução Visual Studio
├── Acheron.vcxproj                # Projeto Visual Studio
├── main.cpp                        # Entry point
├── .gitignore                      # Arquivos ignorados pelo Git
├── README.md                       # Este arquivo
├── LICENSE                         # Licença MIT
├── src/
│   ├── core/
│   │   ├── syscalls_advanced.cpp   # Syscall Manager
│   │   ├── syscalls_advanced.h
│   │   ├── memory_manager.cpp      # Gerenciamento de memória
│   │   ├── memory_manager.h
│   │   ├── pe_loader.cpp           # PE Loader
│   │   ├── pe_loader.h
│   │   ├── c2_client.cpp           # Cliente C2
│   │   └── c2_client.h
│   ├── evasion/
│   │   ├── anti_edr.cpp            # Camada de evasão
│   │   ├── anti_edr.h
│   │   ├── module_stomping.cpp     # Module Stomping
│   │   ├── module_stomping.h
│   │   ├── code_cave.cpp           # Code Caves
│   │   └── code_cave.h
│   ├── payloads/
│   │   ├── stealth_scanner.cpp     # Scanner de credenciais
│   │   ├── stealth_scanner.h
│   │   └── payload_example.cpp     # Exemplo de payload (DLL)
│   └── utils/
│       ├── logger.cpp              # Sistema de log
│       ├── logger.h
│       ├── config.cpp              # Configuração
│       └── config.h
└── payloads/
    └── example_payload.dll         # Payload compilado (DLL)
```

---

## 📚 Referências e Estudos

Este projeto foi construído com base nos seguintes recursos:

### 📖 Livros
- **Windows Internals** (Russinovich, Solomon, Ionescu)
- **Practical Malware Analysis** (Sikorski, Honig)
- **The Art of Memory Forensics** (Ligh, Case, Levy, Walters)

### 🎓 Cursos
- **Sektor7** — Malware Development Essentials + Intermediate
- **Zero-Point Security** — CRTO (Certified Red Team Operator)
- **OffSec** — OSED (Exploit Developer)

### 🌐 Frameworks e Padrões
- **MITRE ATT&CK** — https://attack.mitre.org/
- **Cyber Kill Chain** — Lockheed Martin
- **PTES** — Penetration Testing Execution Standard

### 🛠️ Ferramentas
- **Sysinternals Suite** — Process Monitor, Process Explorer
- **x64dbg** — Debugger
- **WinDbg** — Kernel debugging
- **IDA Pro** / **Ghidra** — Reverse engineering

---

## ⚠️ Aviso Legal

Este projeto foi desenvolvido **exclusivamente para fins educacionais, de pesquisa em segurança ofensiva e aprimoramento técnico**.

**O uso indevido é de inteira responsabilidade do usuário.**

- ❌ **Não use** em sistemas sem autorização explícita
- ❌ **Não use** para atividades ilegais
- ✅ **Use** em ambientes controlados (VMs, laboratórios)
- ✅ **Use** para aprender sobre Windows Internals e segurança ofensiva

Todas as técnicas implementadas neste projeto são **publicamente documentadas** e fazem parte do **conhecimento comum** da comunidade de segurança.

---

## 📄 Licença

Este projeto está sob a **MIT License**. Veja o arquivo [LICENSE](LICENSE) para mais detalhes.

---

## 👤 Autor

**João Pedro**

- GitHub: [@joaopedrofons](https://github.com/joaopedrofons)
- LinkedIn: [seu perfil](https://linkedin.com/in/seu-perfil)

---

## 🌟 Agradecimentos

- À comunidade de segurança ofensiva
- Aos pesquisadores que compartilham conhecimento publicamente
- A todos que contribuíram para o avanço da cibersegurança

---

<p align="center">
  <b>Construído com 🖤 em C++ puro</b>
</p>

<p align="center">
  <i>"The quieter you become, the more you are able to hear."</i>
</p>
