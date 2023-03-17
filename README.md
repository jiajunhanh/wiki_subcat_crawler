# wiki_subcat_crawler

Fetch all wikipedia pages from categories recursively.

```console
Usage: ./wiki_subcat_crawler <category> [<category> ...]
```

Download enwiki-latest-categorylinks.sql and enwiki-latest-page.sql from https://dumps.wikimedia.org/enwiki/latest/ and
import them to a database.

Install [MySQL Connector/C++](https://github.com/mysql/mysql-connector-cpp).

Configure the config.ini for your database. The four lines are address, username, password, and database name,
respectively.

If you use Mac or Linux, you have to configure CMakeLists.txt to correctly link the Connctor/C++.
