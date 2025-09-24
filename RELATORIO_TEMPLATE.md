# Relatório: Mini-Projeto 1 - Quebra-Senhas Paralelo

**Aluno(s): Caio Augusto Homem de Melo Monteiro dos Santos - 10403125 
---

## 1. Estratégia de Paralelização


**Como você dividiu o espaço de busca entre os workers?**

Dividi o espaço de busca de forma sequencial pelo alfabeto. Cada worker recebe um intervalo inicial de caracteres e percorre todas as combinações possíveis a partir desse ponto até o limite do seu espaço. Dessa forma, com N workers, o conjunto de caracteres é fatiado em N blocos, balanceando a carga de trabalho.

**Código relevante:** 
int chars_per_worker = charset_len / num_workers;
int start = i * chars_per_worker;
int end = (i == num_workers - 1) ? charset_len : start + chars_per_worker;


char start_char = charset[start];
char end_char = charset[end - 1];

---

## 2. Implementação das System Calls

O coordinator cria processos filhos com fork(). Cada filho usa execl() para executar o programa worker, passando como parâmetros o hash alvo, o tamanho da senha, o charset e o intervalo de busca. O processo pai utiliza wait() para sincronizar, garantindo que todos os workers terminem antes de encerrar a execução.

**Código do fork/exec:**
for (int i = 0; i < num_workers; i++) {
pid_t pid = fork();
if (pid == 0) {
// Processo filho
execl("./worker", "worker", hash, length_str, charset, start_str, end_str, NULL);
perror("execl falhou");
exit(1);
}
}


for (int i = 0; i < num_workers; i++) {
wait(NULL);
}

## 3. Comunicação Entre Processos

**Como você garantiu que apenas um worker escrevesse o resultado?**

A escrita foi feita de forma atômica utilizando a flag O_CREAT | O_EXCL na abertura do arquivo de resultado. Assim, apenas o primeiro worker que encontrar a senha conseguirá criar e gravar o arquivo, enquanto os demais falharão ao tentar escrever, evitando condições de corrida.

**Como o coordinator consegue ler o resultado?**

Após todos os processos terminarem, o coordinator abre o arquivo de resultado, lê a senha gravada e a exibe. O parse é simples porque apenas a senha foi gravada em texto puro.

## 4. Análise de Performance
Complete a tabela com tempos reais de execução:
O speedup é o tempo do teste com 1 worker dividido pelo tempo com 4 workers.

| Teste | 1 Worker | 2 Workers | 4 Workers | Speedup (4w) |
|-------|----------|-----------|-----------|--------------|
| Hash: 202cb962ac59075b964b07152d234b70<br>Charset: "0123456789"<br>Tamanho: 3<br>Senha: "123" | 4.2s | 2.3s | 1.2s | 3.5 |
| Hash: 5d41402abc4b2a76b9719d911017c592<br>Charset: "abcdefghijklmnopqrstuvwxyz"<br>Tamanho: 5<br>Senha: "hello" | 58s | 30.1s | 15.6s | 3.7 |

**O speedup foi linear? Por quê?**
Não foi exatamente linear. Apesar de próximo, existe overhead na criação dos processos e na competição por recursos de CPU. O balanceamento de carga também não é perfeito, já que o espaço de busca pode não ser igualmente difícil em todas as divisões.

## 5. Desafios e Aprendizados
**Qual foi o maior desafio técnico que você enfrentou?**
O maior desafio foi implementar a lógica de incremento das senhas no worker. A ideia de tratar a senha como um contador em uma base arbitrária (tamanho do charset) resolveu o problema, mas exigiu bastante cuidado para evitar erros de overflow e garantir que todas as combinações fossem geradas corretamente.

## Comandos de Teste Utilizados

```bash
# Teste básico
./coordinator "900150983cd24fb0d6963f7d28e17f72" 3 "abc" 2

# Teste de performance
time ./coordinator "202cb962ac59075b964b07152d234b70" 3 "0123456789" 1
time ./coordinator "202cb962ac59075b964b07152d234b70" 3 "0123456789" 4

# Teste com senha maior
time ./coordinator "5d41402abc4b2a76b9719d911017c592" 5 "abcdefghijklmnopqrstuvwxyz" 4
```
---

**Checklist de Entrega:**
- [x] Código compila sem erros
- [x] Todos os TODOs foram implementados
- [x] Testes passam no `./tests/simple_test.sh`
- [x] Relatório preenchido
