/*
================================================================================
FICHIER: shell.h
VERSION: 2.1 - Avec support historique commandes
================================================================================
*/

#ifndef SHELL_H
#define SHELL_H

void init_shell(void);
void shell_handle_key(char c);

/* NOUVEAU: Navigation dans l'historique des commandes */
void shell_history_up(void);
void shell_history_down(void);

#endif
