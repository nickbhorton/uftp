#!/bin/bash

cmp clie/512b.serv serv/512b.serv
cmp clie/512b.clie serv/512b.clie

cmp clie/1kb.serv serv/1kb.serv
cmp clie/1kb.clie serv/1kb.clie

cmp clie/1mb.serv serv/1mb.serv
cmp clie/1mb.clie serv/1mb.clie

cmp clie/100mb.serv serv/100mb.serv
cmp clie/100mb.clie serv/100mb.clie
