### LaunchApps - Application Launcher Plugin for LXPanel
### LaunchApps é um plugin que mostra uma janela em tela cheia listando todos os programas instalados, e possui um campo de busca

LaunchAppd é idealizado para permitir a busca rápida e execução dos programas instalados, o aspecto visual e funcionamento são baseados no conceito do [Slingscold do Ubuntu](https://sourceforge.net/projects/slingscold/).

A linguagem é C.

O plugin será instalado em: <b>/usr/lib/x86_64-linux-gnu/lxpanel/plugins</b>

##### Referências:
* <https://developer.gnome.org/gio/stable/GAppInfo.html>
* Tutorial referência: <http://wiki.lxde.org/en/How_to_write_plugins_for_LXPanel>

##### Dependências:
	Para Ubuntu e Debian Stretch:
	sudo apt-get install lxpanel-dev libglib2.0-dev libgtk2.0-dev libfm-dev libmagickwand-dev

	Para Debian Jessie:(**Foi testado apenas na versão 0.9.3 do LXPanel)
	sudo apt-get install libglib2.0-dev libgtk2.0-dev libfm-dev libmagickwand-dev
	
Para atualizar o LXPanel para a versão 0.9.3 no **Debian Jessie** faça a instalação dos pacotes:

1. **sudo apt-get install libkeybinder0 libfm-gtk-dev**
2. **lxmenu-data_0.1.5-2_all.deb** <https://packages.debian.org/stretch/lxmenu-data> 
3. **lxpanel-data_0.9.3-1_all.deb** <https://packages.debian.org/stretch/lxpanel-data>
4. **xkb-data_2.19-1_all.deb** <https://packages.debian.org/stretch/xkb-data>
5. **lxpanel_0.9.3-1_amd64.deb** <https://packages.debian.org/stretch/lxpanel>
6. **lxpanel-dev_0.9.3-1_amd64.deb** <https://packages.debian.org/stretch/amd64/lxpanel-dev/download>
	
#### Para instalar pelo source:
##### Importante: O path: /usr/lib/x86_64-linux-gnu/lxpanel/plugins é o local de instalação dos plugins do LXPanel
##### Usei o Eclipse Oxygen(4.7) para implementar o projeto
	git clone https://github.com/acamargovieira/launchapps.git
	cd launchapps
	autoreconf -f
	./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu/lxpanel/plugins
	make
	sudo make install
	
#### Para adicionar ao LXPanel:

![lxpanel-add-tr](https://user-images.githubusercontent.com/20074560/32780937-3afa54a8-c92a-11e7-83fa-b36363e02723.png)

##### LaunchApps

**Teclas de atalho:**

* Esc &rArr; Fecha

* Enter &rArr; Executa a busca

* &uarr; &rArr; Avança uma página

* &darr; &rArr; Retrocede uma página

* Roda do mouse &rArr; Para frente: avança uma página, para trás: retrocede uma página

**Cliques**

* Clique sobre o ícone do programa &rArr; Executa o programa e fecha a janela do LaunchApps

* ![indicators](https://user-images.githubusercontent.com/20074560/32782503-c9547f4e-c92f-11e7-939a-6676a6385857.png) &rArr; Avança ou retrocede uma página

* ![lupa](https://user-images.githubusercontent.com/20074560/32782302-1cb73ca4-c92f-11e7-9e7f-e72230a0b06d.png) &rArr; Executa a busca

* ![clear](https://user-images.githubusercontent.com/20074560/32782376-58e7315c-c92f-11e7-8530-20f1c9efb382.png) &rArr; limpa o campo de busca e volta para o ínicio
 
![launchapps](https://user-images.githubusercontent.com/20074560/32780952-463bb7bc-c92a-11e7-9013-ddd843ed0ac4.gif)

#### Testado no LXPanel versão 0.9.3

#### Não testado no LXPanel 0.8.2

#### Não testado no LXPanel 0.7.2

## CHANGELOG

### [Unreleased]

### [1.0.1] - 2017-11-16
#### Fixed
- Dynamic calculation of pages

### [1.0.0] - 2017-11-14
- Release 1.0.0





