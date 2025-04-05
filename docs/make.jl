using Documenter
import Documenter.Remotes.GitHub
import Documenter.GitHubActions

const Manual = [
    "manual/getting-started.md",
    "manual/constants.md",
]

if "pdf" in ARGS
const PAGES = [
    "Manual" => ["index.md", Manual...],
]
else
const PAGES = [
    "Babel Documentation" => "index.md",
    "Manual" => Manual,
]
end

const FORMAT = if "pdf" in ARGS
    Documenter.LaTeX()
else
    Documenter.HTML(;
        prettyurls = ("deploy" in ARGS),
        canonical = ("deploy" in ARGS) ? "https://wehrwolff.github.io/babel/v0.0" : nothing,
        collapselevel = 1,
        sidebar_sitename = true,
        warn_outdated = true,
        inventory_version = "0.0",
        footer = "Powered by [Documenter.jl](https://github.com/JuliaDocs/Documenter.jl) and the [Babel Programming Language](https://github.com/WehrWolff/babel).",
    )
end

makedocs(;
    source = "src",
    build = "build",
    clean = !("deploy" in ARGS),
    repo = GitHub("WehrWolff", "babel"),
    checkdocs = :none,
    format = FORMAT,
    sitename = "The Babel Programming Language",
    authors = "WehrWolff",
    pages = PAGES,
)

if "deploy" in ARGS
    deploydocs(
        repo = GitHub("WehrWolff", "babel"),
        devbranch = "main",
        target = "build",
        branch = "gh-pages",
        deploy_config = GitHubActions(),
        versions = Versions(["v#.#"])
    )
else
    @info "Skipping deployment ('deploy' not passed)"
end