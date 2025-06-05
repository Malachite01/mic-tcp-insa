# BE-RESEAU-MIC-TCPV2

MIC-TCP est une implémentation simplifiée du protocole TCP écrite en C. Ce projet a pour but de simuler les mécanismes de base de TCP (numéros de séquence, acquittements, gestion des connexions, etc.).

## 📌 Objectifs

- Implémenter une pile TCP minimale au-dessus d’une couche IP simulée
- Gérer la création de sockets, la liaison (`bind`), la connexion (`connect`)
- Assurer l’envoi et la réception de données avec fiabilité (modèle Stop & Wait)
- Gérer les acquittements (ACK) et les numéros de séquence
- Simuler des pertes de paquets et prévoir un retransfert
- Comprendre les principes de base des protocoles orientés connexion

## 🧱 Architecture

Le projet est structuré autour de plusieurs fonctions principales :

### Initialisation et gestion des sockets

- `mic_tcp_socket()`: Crée un socket MIC-TCP
- `mic_tcp_bind()`: Lie une adresse locale à un socket
- `mic_tcp_connect()`: Établit une connexion à un hôte distant
- `mic_tcp_accept()`: Accepte une connexion entrante (modèle simplifié)
- `mic_tcp_close()`: Ferme un socket et libère les ressources

### Transmission de données

- `mic_tcp_send()`: Envoie une donnée avec gestion des numéros de séquence et des acquittements
- `mic_tcp_recv()`: Reçoit une donnée depuis le buffer applicatif

### Réception des PDU

- `process_received_PDU()`: Fonction appelée à la réception d’un PDU MIC-TCP. Elle traite le numéro de séquence, stocke les données, et envoie un ACK si nécessaire.

## 🔍 Validation et sécurité

Deux fonctions vérifient la validité des entrées :

- `verif_socket()`: Vérifie la validité d’un socket (bornes, existence)
- `verif_address()`: Vérifie qu’une adresse IP contient **4 segments entre 0 et 255** et que le **port > 1024**

## 🧪 Simulateur réseau

La communication IP simulée est assurée par des appels à :

- `IP_send(pdu, ip_addr)`
- `IP_recv(pdu, local_addr, remote_addr, timeout)`

Le taux de perte peut être configuré avec `set_loss_rate()` pour tester la fiabilité du protocole.

## 📁 Dépendances

- `mictcp.h` : Interface de programmation principale
- `api/mictcp_core.h` : Contient les appels à la couche IP simulée
- `lib-mictcp` : Composants internes simulés (buffers, IP, logiques réseau)

## 🛠 Compilation

Inclure les fichiers `.c` nécessaires dans un Makefile ou compiler manuellement :

```bash
make
```

## 📚 Exemple d'utilisation
Ce programme peut être testé avec tsock (en texte ou vidéo). Pour tester le puits (serveur) en mode texte, avec le port 9000:
```bash
./tsock_texte -p 9000
```

Pour envoyer un message au puits :
```bash
./tsock_texte -s 127.0.0.1 9000
```
puis en mode interactif, vous pouvez envoyer des messages.

pour la vidéo :
//TODO

## 👨‍💻 Auteurs
Projet réalisé dans le cadre du module [BE Réseaux] a l'INSA Toulouse.
Mathieu ANTUNES
Enzo MARIETTI