void movestack(const Arg * arg)
{
	Client *c = nextclient(arg->i, 1), *p = NULL, *pc = NULL, *i;

	if (!selmon->sel)
		return;

	/* find the client before selmon->sel and c */
	for (i = selmon->clients; i && (!p || !pc); i = i->next)
	{
		if (i->next == selmon->sel)
			p = i;
		if (i->next == c)
			pc = i;
	}

	/* swap c and selmon->sel selmon->clients in the selmon->clients list */
	if (c && c != selmon->sel)
	{
		Client *temp = selmon->sel->next == c ? selmon->sel : selmon->sel->next;
		selmon->sel->next = c->next == selmon->sel ? c : c->next;
		c->next = temp;

		if (p && p != c)
			p->next = c;
		if (pc && pc != selmon->sel)
			pc->next = selmon->sel;

		if (selmon->sel == selmon->clients)
			selmon->clients = c;
		else if (c == selmon->clients)
			selmon->clients = selmon->sel;

		arrange(selmon);
	}
}
