**Mozc team is not accepting pull requests at the moment.**

Although Google company policy certainly allows Mozc team to accept pull
requests, to do so Mozc team needs to move all Mozc source files into
`third_party` directory in the Google internal source repository [1].
Doing that without breaking any Google internal project that depends on
Mozc source code requires non-trivial amount of time and engineering
resources that Mozc team cannot afford right now.

Mozc team continues to seek opportunities to address this limitation,
but we are still not ready to accept any pull request due to the above
reason.

[1]: [Open Source at Google - Linuxcon 2016](http://events.linuxfoundation.org/sites/events/files/slides/OSS_at_Google.pdf#page=30)
> ### License Compliance
> - We store all external open source code in a third_party hierarchy,
> along with the licenses for each project. We only allow the use of OSS
> under licenses we can comply with.
> - Use of external open source is not allowed unless it is put in that
> third_party tree.
> - This makes it easier to ensure we are only using software with
licenses that we can abide.
> - This also allows us to generate a list of all licenses used by any
project we build when they are released externally.
