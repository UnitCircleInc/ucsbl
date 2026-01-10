# Running GitHub Actions Locally

Install [act](https://github.com/nektos/act)
Install [docker](https://docker.com)

> For Linux, you need [Docker Engine](https://docs.docker.com/engine/install/ubuntu/)  not Docker Desktop.

Then run:

```
$ act -j build
```



## Creating a Github Token (not needed at this time)

Create a Github Personal Access Token (classic) [Settings, Developer Settings, Tokens](https://github.com/settings/tokens).

* Select `Generate new token`.
* Select `Generate new token (classic)`.
* Ensure the `repo` check box is selected (leave other changed).
* Click `Generate`
* Copy the copy token to a safe place `ghp_xxxxxxxxxxx...`
