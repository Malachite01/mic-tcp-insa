## 📑 Sommaire

- [BE-RESEAU-MIC-TCPV2](#be-reseau-mic-tcpv2)
  - [📌 Objectifs](#-objectifs)
  - [🚧 Versions Implémentées](#-versions-implémentées)
    - [✅ Version 1 : MICTCP-v1](#-version-1--mictcp-v1)
    - [✅ Version 2 : MICTCP-v2](#-version-2--mictcp-v2)
    - [✅ Version 3 : MICTCP-v3 (WIP 🚧)](#-version-3--mictcp-v3-wip-)
  - [🛠 Compilation](#-compilation)
  - [📚 Exemple d'utilisation](#-exemple-dutilisation)
    - [Texte](#texte)
    - [Vidéo](#vidéo)
  - [🧱 Architecture](#-architecture)
    - [Initialisation et gestion des sockets](#initialisation-et-gestion-des-sockets)
    - [Transmission de données](#transmission-de-données)
    - [Réception des PDU](#réception-des-pdu)
  - [🔍 Validation et sécurité](#-validation-et-sécurité)
  - [🧪 Simulateur réseau](#-simulateur-réseau)
  - [📁 Dépendances](#-dépendances)
  - [👨‍💻 Auteurs](#-auteurs)


# BE-RESEAU-MIC-TCPV2

MIC-TCP est une implémentation simplifiée du protocole TCP écrite en C, et qui prend en compte un taux de perte. Ce projet a pour but de simuler les mécanismes de base de TCP (numéros de séquence, acquittements, gestion des connexions, etc.).


## 📌 Objectifs

- Implémenter une pile TCP minimale au-dessus d’une couche IP simulée
- Gérer la création de sockets, la liaison (`bind`), la connexion (`connect`)
- Assurer l’envoi et la réception de données avec fiabilité (modèle Stop & Wait)
- Gérer les acquittements (ACK) et les numéros de séquence
- Simuler des pertes de paquets et prévoir un retransfert
- Comprendre les principes de base des protocoles orientés connexion


## 🚧 Versions Implémentées
### ✅ Version 1 : MICTCP-v1
Fonctionnalité : Transfert de données sans fiabilité

- ✔️ Primitives mic_tcp_socket, mic_tcp_bind, mic_tcp_send, mic_tcp_recv fonctionnelles
- ✔️ Aucune retransmission en cas de perte
- ✔️ Pas de synchronisation application/transport

### ✅ Version 2 : MICTCP-v2
Fonctionnalité : Mécanisme de fiabilité totale avec Stop & Wait

- ✔️ Chaque PDU est accusé réception (ACK) avant d’émettre le suivant
- ✔️ Implémentation d’un numéro de séquence
- ✔️ Fonction mic_tcp_recv bloquante jusqu'à réception du message attendu
- ✔️ Table de réception utilisée pour la gestion des messages
- ✔️ Fiabilité stricte : le socket réémet indéfiniment les paquets tant que l’ACK attendu n’est pas reçu, sans tolérer aucune perte.

### ✅ Version 3 : MICTCP-v3 (WIP 🚧)
Fonctionnalité : Mécanisme de fiabilité avec taux de perte configurable

- ✔️ Ajout d’un taux de perte configurable pour simuler des erreurs réseau (utilisation de la fonction `set_loss_rate()`)
- ✔️ Gestion des retransmissions en cas de perte simulée
- ✔️ Utilise des tableaux pour tracker les paquets envoyés et les ACK reçus (fenêtre glissante)
- ✔️ Calcul du taux de perte basé sur une fenêtre glissante a taille définie
- ✔️ Logique de fiabilité partielle dans mic_tcp_send : 
    - Envoi normal avec attente d'ACK, Si pas d'ACK: évaluation du taux de perte
    -> Si taux acceptable: continuer, et ne pas incrémenter le numéro de séquence
    -> Si taux trop élevé: continuer les retransmissions
> [!NOTE]  
> *Pourquoi n’incrémente-t-on pas le numéro de séquence en cas de perte acceptable ?*
>Lorsqu’un ACK est perdu, cela peut provoquer une désynchronisation entre les numéros de séquence du client (source) et du serveur (puits).

>En effet, si le puits reçoit correctement une donnée, il incrémente son numéro de séquence et renvoie un ACK. Mais si cet ACK est perdu, la source ne le reçoit pas et n’incrémente donc pas son propre numéro de séquence.

>Si le taux de perte reste dans une limite acceptable, aucune retransmission n’est déclenchée. La source croit alors que la donnée n’a pas été reçue, alors qu’elle l’a bien été.

>Ce désalignement n’est pas critique. Lors d’un envoi suivant où tout se passe correctement, la source reçoit un ACK et incrémente son numéro de séquence, tandis que le puits, constatant un numéro déjà vu, ignore la donnée et renvoie simplement un ACK.

>Cela permet une resynchronisation naturelle des numéros de séquence.
>En d’autres termes, le second échange "corrige" le désalignement du premier.


## 🛠 Compilation

Pour compiler, le Makefile sera utilisé :

```bash
make
```


## 📚 Exemple d'utilisation
> [!NOTE]  
> Il est possible que lors de l'exécution de tsock (en mode texte ou vidéo), vous rencontriez une erreur avec le charactère `^M`
> Pour corriger cela, vous pouvez utiliser l'utilitaire `dos2unix` pour convertir les fichiers en format Unix :
> ```bash
> dos2unix tsock_texte
> dos2unix tsock_video
> ```

###	Texte
Ce programme peut être testé avec tsock (en texte ou vidéo). Pour tester le puits (serveur) en mode texte, avec le port 9000:
Pour démarrer le puits (serveur) :
```bash
./tsock_texte -p 9000
```

Pour envoyer un message au puits (client), vous pouvez utiliser la commande suivante :
```bash
./tsock_texte -s 127.0.0.1 9000
```
puis en mode interactif, vous pouvez envoyer des messages.

### Vidéo
> [!WARNING]  
> Dans le cas ou vous obtenez l'erreur `src/apps/gateway.c:344 [read_rtp_packet()] -> Buffer is too small to store the packet`, il s'agit d'un problème de lien symbolique, que vous pouvez corriger en recréant les liens de la vidéo que vous voulez visioner :

> [!NOTE]  
> Pour réellement visionner la vidéo, vlc est nécessaire.
> ```bash
> vlc --version
VLC media player 3.0.16 Vetinari (revision 3.0.13-8-g41878ff4f2)
VLC version 3.0.16 Vetinari (3.0.13-8-g41878ff4f2)
Compiled by buildd on lcy02-amd64-108.buildd (Mar 13 2022 08:00:10)
Compiler: gcc version 11.2.0 (Ubuntu 11.2.0-18ubuntu1)
This program comes with NO WARRANTY, to the extent permitted by law.
You may redistribute it under the terms of the GNU General Public License;
see the file named COPYING for details.
Written by the VideoLAN team; see the AUTHORS file.
> ```

```bash
rm video.bin
ln -s video_starwars.bin video.bin 
```
Pour tester avec tsock en mode vidéo avec notre protocole mictcp, avec la commande suivante :
Pour démarrer le puits (serveur) :
```bash
./tsock_video -p -t mictcp 9000
```

Pour envoyer lancer la vidéo (client):
```bash
./tsock_video -s -t mictcp 9000
```	

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


## 👨‍💻 Auteurs
Projet réalisé dans le cadre du module [BE Réseaux] a l'INSA Toulouse.
Mathieu ANTUNES
Enzo MARIETTI

[Code non complété du projet](https://github.com/rezo-insat/mictcp)
